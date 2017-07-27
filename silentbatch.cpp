/*
 * silentbatch.  Based on public-domain code written by Paul Miner.
 * Rewritten in C++ by James D. Lin.
 */

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <vector>
#include <string>
#include <sstream>

#include <windows.h>
#include <winuser.h>
#include <shellapi.h>

#define APP_NAME L"silentbatch"


/** CHandle
  *
  *     A class to manage Windows HANDLEs.  Note that some Windows
  *     functions use NULL as an invalid handle value and some use
  *     INVALID_HANDLE_VALUE, so we must parameterize on the invalid
  *     handle value.
  */
template<HANDLE invalidHandleValue>
class CHandle
{
public:
    explicit CHandle(HANDLE h = invalidHandleValue)
    : mHandle(h)
    {
    }

    ~CHandle()
    {
        close();
    }

    void close()
    {
        if (mHandle != invalidHandleValue)
        {
            CloseHandle(mHandle);
        }

        release();
    }

    void release()
    {
        mHandle = invalidHandleValue;
    }

    HANDLE get() const
    {
        return mHandle;
    }

    HANDLE* getAddress()
    {
        return &mHandle;
    }

    CHandle& operator=(HANDLE h)
    {
        close();
        mHandle = h;
        return *this;
    }

    operator bool() const
    {
        return mHandle != invalidHandleValue;
    }

private:
    HANDLE mHandle;

private:
    // Non-copyable.
    CHandle(const CHandle&);
    CHandle& operator=(const CHandle&);
};


/** LocalAllocGuard
  *
  *     A class to manage memory to be freed via LocalFree.
  */
class LocalAllocGuard
{
public:
    explicit LocalAllocGuard(void* p = NULL)
    : mP(p)
    {
    }

    ~LocalAllocGuard()
    {
        LocalFree(mP);
    }

private:
    void* mP;

private:
    // Non-copyable.
    LocalAllocGuard(const LocalAllocGuard&);
    LocalAllocGuard& operator=(const LocalAllocGuard&);
};


struct PipeHandles
{
    CHandle<NULL> hRead;
    CHandle<NULL> hWrite;
};


/** GetErrorMessage
  *
  *     Retrieves the system error message associated with a given error
  *     code.
  *
  * PARAMETERS:
  *     err : The error code.
  *
  * RETURNS:
  *     The error message.
  */
static std::wstring
GetErrorMessage(DWORD err)
{
    std::wstring::value_type* p;
    if (FormatMessageW(  FORMAT_MESSAGE_ALLOCATE_BUFFER
                       | FORMAT_MESSAGE_IGNORE_INSERTS
                       | FORMAT_MESSAGE_FROM_SYSTEM,
                       NULL, err, 0, reinterpret_cast<std::wstring::value_type*>(&p), 0, NULL)
        != 0)
    {
        LocalAllocGuard g(p);
        return std::wstring(p);
    }
    else
    {
        return std::wstring();
    }
}


/** DebugOutput
  *
  *     Generates debugging output.
  *
  * PARAMETERS:
  *     IN message : The message.
  */
static void
DebugOutput(const std::wstring& message)
{
    std::wstringstream ss;
    ss << APP_NAME << ": " << message;
    OutputDebugStringW(ss.str().c_str());
}


/** OutputError
  *
  *     Generates an error dialog.
  *
  * PARAMETERS:
  *     IN message : The main error message.
  *     error      : The error code.
  */
static void
OutputError(const std::wstring& message, DWORD error)
{
    std::wstringstream ss;
    ss << message << L" (" << error << L")";
    DebugOutput(ss.str());

    const std::wstring s = message + L"\n" + GetErrorMessage(error);
    MessageBoxW(NULL, s.c_str(), APP_NAME, MB_ICONERROR | MB_OK);
}


/** CreateChildProcess
  *
  *     Creates a specified child process.
  *
  * PARAMETERS:
  *     IN cmdLine     : The command line to invoke the child process.
  *     hChildStdOutWr : Pipe handle that the child process should use when
  *                        writing to stdout or to stderr.
  *     hChildStdinRd  : Pipe handle that the child process should use when
  *                        reading from stdin.
  *
  * RETURNS:
  *     A handle to the created process.
  *     Returns NULL on failure.
  */
static HANDLE
CreateChildProcess(const std::wstring& cmdLine,
                   HANDLE hChildStdOutWr, HANDLE hChildStdInRd)
{
    PROCESS_INFORMATION piProcInfo = { 0 };
    STARTUPINFOW siStartInfo = { 0 };
    siStartInfo.cb = sizeof siStartInfo;
    siStartInfo.hStdError = hChildStdOutWr;
    siStartInfo.hStdOutput = hChildStdOutWr;
    siStartInfo.hStdInput = hChildStdInRd;
    siStartInfo.dwFlags = STARTF_USESTDHANDLES;

    // Create the child process.
    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    BOOL success = CreateProcessW(NULL,
                                  &cmdLineBuf[0],    // command line
                                  NULL,              // process security attributes
                                  NULL,              // primary thread security attributes
                                  TRUE,              // handles are inherited
                                  CREATE_NO_WINDOW,  // creation flags
                                  NULL,              // use parent's environment
                                  NULL,              // use parent's current directory
                                  &siStartInfo,      // STARTUPINFO pointer
                                  &piProcInfo);      // receives PROCESS_INFORMATION

    if (success) { CloseHandle(piProcInfo.hThread); }
    return success ? piProcInfo.hProcess : NULL;
}


/** ReadFromPipe
  *
  *     Reads output from the child process and writes it to the parent's
  *     stdout.
  *
  * PARAMETERS:
  *     hProc          : Handle the child process.
  *     hChildStdOutRd : Pipe handle used to read the stdout output from
  *                        the child process.
  *     hParentStdOut  : Pipe handle to use to write to the parent
  *                        process's stdout.  May be INVALID_HANDLE_VALUE.
  */
static void
ReadFromPipe(HANDLE hProc, HANDLE hChildStdOutRd, HANDLE hParentStdOut)
{
    std::vector<char> buf(4096);

    DWORD waitResult;
    do
    {
        // Don't bother using WaitForMultipleObjects with hChildStdOutRd.
        // Even if nothing's being actively written to it, it apparently
        // gets signalled so frequently that it causes large CPU spikes.
        // Poll on a 100 ms timeout instead.  Alas.
        waitResult = WaitForSingleObject(hProc, 100);

        DWORD available;
        if (!PeekNamedPipe(hChildStdOutRd, NULL, 0, NULL, &available, NULL))
        {
            break;
        }

        if (available > 0)
        {
            if (available > buf.size()) { buf.resize(available); }

            DWORD dwRead;
            if (   !ReadFile(hChildStdOutRd, &buf[0], available, &dwRead, NULL)
                || dwRead == 0)
            {
                break;
            }

            if (hParentStdOut != INVALID_HANDLE_VALUE)
            {
                DWORD dwWritten;
                (void) WriteFile(hParentStdOut, &buf[0], dwRead, &dwWritten, NULL);
                FlushFileBuffers(hParentStdOut);
            }
        }
    } while (waitResult == WAIT_TIMEOUT);
}


/** ParseCommandLine
  *
  *     Parses the command-line.
  *
  * RETURNS:
  *     A std::vector containing the command-line arguments.
  */
static std::vector<std::wstring>
ParseCommandLine()
{
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    LocalAllocGuard g(argv);

    return std::vector<std::wstring>(argv, argv + argc);
}


int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
        LPSTR lpCmdLine, int nCmdShow)
{
    CHandle<INVALID_HANDLE_VALUE> hStdOut;

    std::vector<std::wstring> args = ParseCommandLine();
    if (args.size() < 2)
    {
        MessageBoxW(NULL,
                    APP_NAME L" v1.1.  By Paul Miner and James D. Lin.\n"
                    L"http://www.taenarum.com/software/\n"
                    L"\n"
                    L"Usage: " APP_NAME L" BATCHFILE [LOGFILE]\n",
                    APP_NAME,
                    MB_ICONINFORMATION | MB_OK);
        return EXIT_SUCCESS;
    }

    if (args.size() > 2)
    {
        hStdOut = CreateFileW(args[2].c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        // Always append to an existing file.
        if (   hStdOut
            && SetFilePointer(hStdOut.get(), 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER
            && GetLastError() != NO_ERROR)
        {
            hStdOut.close();
        }

        if (!hStdOut)
        {
            OutputError(L"Failed to open log file " + args[2] + L".", GetLastError());
            return EXIT_FAILURE;
        }
    }

    SECURITY_ATTRIBUTES saAttr = { 0 };
    saAttr.nLength = sizeof saAttr;
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    PipeHandles childStdOut;
    PipeHandles childStdIn;

    CHandle<NULL> hChildStdOutRdCopy;
    CHandle<NULL> hChildStdInWrCopy;

    // Create a pipe for the child process's stdout.
    if (!CreatePipe(childStdOut.hRead.getAddress(),
                    childStdOut.hWrite.getAddress(),
                    &saAttr, 0))
    {
        OutputError(L"Failed to create stdout pipe for child process.", GetLastError());
        return EXIT_FAILURE;
    }

    // Create a pipe for the child process's stdin.
    if (!CreatePipe(childStdIn.hRead.getAddress(),
                    childStdIn.hWrite.getAddress(),
                    &saAttr, 0))
    {
        OutputError(L"Failed to create stdin pipe for child process.", GetLastError());
        return EXIT_FAILURE;
    }

    // Create a non-inheritable version of the stdout pipe's read handle.
    if (!DuplicateHandle(GetCurrentProcess(), childStdOut.hRead.get(),
                         GetCurrentProcess(), hChildStdOutRdCopy.getAddress(),
                         0, FALSE, // Not inherited.
                         DUPLICATE_SAME_ACCESS))
    {
        OutputError(L"Failed to duplicate stdout handle.", GetLastError());
        return EXIT_FAILURE;
    }
    childStdOut.hRead.close();

    // Create a non-inheritable version of the stdin pipe's write handle.
    if (!DuplicateHandle(GetCurrentProcess(), childStdIn.hWrite.get(),
                         GetCurrentProcess(), hChildStdInWrCopy.getAddress(),
                         0, FALSE, // Not inherited.
                         DUPLICATE_SAME_ACCESS))
    {
        OutputError(L"Failed to duplicate stdin handle.", GetLastError());
        return EXIT_FAILURE;
    }
    childStdIn.hWrite.close();

    // Now create the child process.
    const std::wstring cmdLine = L"\"" + args[1] + L"\"";
    CHandle<NULL> hProc(CreateChildProcess(cmdLine,
                                           childStdOut.hWrite.get(),
                                           childStdIn.hRead.get()));
    if (!hProc)
    {
        OutputError(L"Failed to create process " + cmdLine + L".", GetLastError());
        return EXIT_FAILURE;
    }
    else
    {
        DebugOutput(L"Spawned: " + cmdLine);
    }

    // Read from the stdout pipe of the child process.
    //
    // Close the write end of the pipe before reading from the read end of
    // the pipe.
    childStdOut.hWrite.close();
    ReadFromPipe(hProc.get(), hChildStdOutRdCopy.get(), hStdOut.get());

    DebugOutput(L"Completed.");

    return EXIT_SUCCESS;
}
