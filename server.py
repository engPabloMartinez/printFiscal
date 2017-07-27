#!/usr/bin/env python
# coding=utf-8

import tornado.ioloop
import signal
import os.path
import logging
import FiscalStarter
from Traductores.TraductoresHandler import TraductoresHandler
from threading import Thread

def sig_handler(sig, frame):
	logging.info('Caught signal: %s', sig)
	tornado.ioloop.IOLoop.instance().add_callback(fbserver.shutdown)

# chdir otherwise will not work fine in rc service
newpath = os.path.dirname(os.path.realpath(__file__))
os.chdir(newpath)		

fbserver = FiscalStarter.FiscalServer()

signal.signal(signal.SIGTERM, sig_handler)
signal.signal(signal.SIGINT, sig_handler)

fbserver.start()