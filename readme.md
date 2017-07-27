# printFiscal

## NOTA IMPORTANTE: Este proyecto es una mejora del proyecto https://github.com/paxapos/fiscalberry

Agregue funcionalidad que no tenia el proyecto antes mencionado (por ejemplo, percepciones, impuestos internos, remitos, descuentos y recargos, etc.)

Funciona en cualquier PC con cualquier sistema operativo que soporte python.

Windows: 
	Se incluye en el proyecto la version del Python sobre la que esta testeada, y ya tiene incluidos todos las dependencias necesarias.
	Ademas se adjunta el instalador del NMap que se necesita instalar. En la carpeta Instalar.
	
Linux:
	Se necesitan estas dependencias de python	
		* python-pip (yum o apt-get)
		* pyserial (pip install)
		* build-essential python-dev (yum o apt-get)
		* tornado (pip install)
		* python-nmap (yum o apt-get)
		* python-imaging python-serial python-dev python-setuptools (yum o apt-get)
		* python-escpos (pip install)

Esta testeado y usandose en un Proyecto en Angular 2, solo hay que llamar a un WS de tipo WebSocket con la url, enviando y recibiendo JSONs.

### Archivo de configuracion

Se incluyen los config.ini de cada uno de los modelos que se soportan, solo hay que renombrar el que se necesite como config.ini.

Las opciones son:

  para marca: [Hasar, Epson]

  modelo: 
  	(para Hasar)
		"615"
		"715v1"
		"715v2"
		"320"

	(para Epson)
		"tickeadoras"
		"epsonlx300+"
		"tm220af"

  path:
  	en windows: (COM1, COM2, etc)
  	en linux: (/dev/ttyUSB0, /dev/ttyS0, etc)

  driver:
  	(puede quedar vacio, en ese caso si la marca setteada es Hasar, el driver sera Hasar, lo mismo con Epson. Modificar File o Dummy es útil para hacer pruebas o desarrollo)
  	Hasar, Epson, Dummy, File

  	En el caso de seleccionar File, en la variable "path" hay que colocar el nombre del archivo que deseo crear para que se escriban las salidas. Ej en linux: "/tmp/archivo.txt"

### Iniciar el servicio

	Windows:
		Se incluye un bat llamado iniciarServicio.bat que ejecuta el daemon.
		Si se quiere ejecutarlo en modo silent (que no abra la ventana negra del cmd), en el ejecutar de windows poner
			silentbatch.exe iniciarServicio.bat
	
	Linux: 
		python server.py
	
### Instalar Daemond
	Windows:
		Añadir un acceso directo a "%appdata%\Microsoft\Windows\Start Menu\Programs\Startup" con el comando	
					silentbatch.exe iniciarServicio.bat
					
	Linux:
		Editar el archivo fiscal-server-rc 
			Este hace que el server.py se convierta en un servicio de linux para ejecutar en background. 
			Antes de instalarlo es necesario modificarle la linea donde se especifica el path en donde esta instalado el proyecto: 
				DIR=aca poner el path a la carpeta			
			deberá ingresar el path correcto ( en el ultimo "/") EJ: DIR=/home/USER/fiscal
			
		cp fiscal-server-rc  /etc/init.d/  
		update-rc.d fiscal-server-rc defaults	
		
		listo, ya lo podemos detener o iniciar usando

		service fiscal-server-rc stop
		service fiscal-server-rc start
		service fiscal-server-rc restart

# Documentación

Al imprimir un ticket el servidor enviará 3 comandos previos que pueden resultar en un mensaje de warning: "comando no es valido para el estado de la impresora fiscal ".
Esto no es un error, sino que antes de imprimir un tiquet envia:
* CANCELAR CUALQUIER TIQUET ABIERTO
* CANCELAR COMPROBANTE NO FISCAL
* CANCELAR NOTA ED CREDITO O DEBITO
Es una comprobación útil que ahorrará dolores de cabeza y posibilidades de bloquear la impresora fiscal.

El JSON siempre tiene que tener este formato

{
 "accion_a_ejecutar": 
 {
    parametros de la accion
 },
 "printerName": "IMPRESORA_FISCAL" // este es el nombre de la impresora del archivo config.ini (siempre tiene que haber una con este nombre minimamente)
}

Lo enviamos usando websockets a un host y puerto determinado (el servidor fiscal), éste lo procesa, envia a imprimir, y responde al cliente con la respuesta de la impresora.

## ACCION printTicket o printRemito		

### Tipos de comprobantes soportados
	tipo_cbte
        "TA", 	#Tiquets A
		"TB",  	#Tiquets B
        "FA", 	#Factura A
        "FB", 	#Factura B
        "NDA", 	#Nota Debito A
        "NCA", 	#Nota Credito A
        "NDB", 	#Nota Debito B
        "NCB", 	#Nota Credito B
        "FC", 	#Factura C
        "NDC", 	#Nota Debito C
        "NDC", 	#Nota Credito C
        "R" 	#Remito
		
### Tipos de documentos
	tipo_doc
		"DNI",
		"CUIT",
		"LIBRETA_CIVICA",
		"LIBRETA_ENROLAMIENTO",
		"PASAPORTE",
		"SIN_CALIFICADOR",
		"CEDULA"

### Tipos de responsable
	tipo_responsable
		"RESPONSABLE_INSCRIPTO",
		"RESPONSABLE_NO_INSCRIPTO",
		"NO_RESPONSABLE",
		"EXENTO",
		"CONSUMIDOR_FINAL",
		"RESPONSABLE_MONOTRIBUTO",
		"NO_CATEGORIZADO",
		"PEQUENIO_CONTRIBUYENTE_EVENTUAL",
		"MONOTRIBUTISTA_SOCIAL",
		"PEQUENIO_CONTRIBUYENTE_EVENTUAL_SOCIAL"

### Partes del JSON a enviar
		encabezado (opcional): array de strings para imprimir en el header del ticket/factura
		
		cabecera (obligatorio): datos del cliente, domicilio, tipo_responsable, tipo_cbte, tipo y nro de doc
			en el caso de las NC se agrega un item "referencia" que es el numero del comprobante original, pero es opcional
		
		items (obligatorio): array con el listado de productos a imprimir
			- el campo importeInternosEpsonB es porque para las Epson la tasa de ajuste para los internos se calcula distinto que en las A (ver el manual de epson.)
			- en los items esta la posibilidad de enviar itemNegative (por defecto false, para enviar en negativo items) 
														 discount (por defecto 0 para enviar descuentos o recargos por item)
														 discountDescription (por defecto '', descripcion del dto recargo del item)
														 discountNegative (por defecto en True (descuentos) sino False (recargos))
		
		dtosGenerales (opcional): descuentos globales o recargos dependiendo el modelo de impresora, para esto se usa el negative
		
		formasPago (opcional): array con las distintas formas de pago del ticket / factura

		percepciones (opcional): percepciones de la factura/ticket 
			para percepciones de iva mandar el campo alic_iva con el % de iva correspondiente
			para otras percepciones no enviar nada o enviar **.**
			
		descuentosRecargos (opcional): descuentos y recargos al final de los items
		
		pie (opcional): array de strings para imprimir en el footer del ticket/factura
		
## EJEMPLOS

### FACTURA A

{
	"printTicket": {
		"encabezado": ["Nombre del vendedor:", "ACOSTA, ELIO", "."],
		"cabecera": {
			"tipo_cbte": "FA",
			"nro_doc": 20064219457,
			"domicilio_cliente": "CALLE DE LA DIRECCION 1234",
			"tipo_doc": "CUIT",
			"nombre_cliente": "HERMIDA MANUEL JORGE",
			"tipo_responsable": "RESPONSABLE_INSCRIPTO"
		},
		"items": [{
			"alic_iva": 21,
			"importe": 128.5749995,
			"ds": "FERNET BRANCA 6 x 750",
			"qty": 24,
			"porcInterno": 21.85,
			"importeInternosEpsonB": 0.1404644765330137
			}, {
				"alic_iva": 21,
				"importe": 164.0160845,
				"ds": "FERNET BRANCA 6 x 1000 cc",
				"qty": 6,
				"porcInterno": 21.85,
				"importeInternosEpsonB": 0.14046323609194558
			}, {
				"alic_iva": 21,
				"importe": 76.8718705,
				"ds": "FERNET BRANCA 12 X 450 CC",
				"qty": 12,
				"porcInterno": 21.85,
				"importeInternosEpsonB": 0.1404704988933501
			}],
		"dtosGenerales": [{
			"alic_iva": 21,
			"importe": 10,
			"ds": "Descuento",
			"negative": true
		}],
		"formasPago": [{
			"ds": "Cuenta Corriente",
			"importe": 4770.22
		}],
		"percepciones": [{
			"importe": 52.22,
			"ds": "PERCEPCION CBA",
			"porcPerc": 2
		}],
		"descuentosRecargos": [{
			"alic_iva": 21,
			"importe": 155.5,
			"ds": "DESCUENTO",
			"porcInterno": 21.85,
			"importeInternosEpsonB": 0.21003215434083602,
			"itemNegative": true
			}],
		"pie": ["Efectivo 4771.22", "Vuelto: 1.00"]
	},
	"printerName": "IMPRESORA_FISCAL"
}

## ACCION openDrawer	

Abre la gaveta de dinero. No es necesario pasar parámetros extra.
{
  "openDrawer": true
}

## ACCION dailyClose	

Imprime un cierre fiscal X o Z dependiendo el parámetro enviado

### Cierre X
{
  "dailyClose": "Z"
}

### Cierre X
{
  "dailyClose": "X"
}

## ACCION getLastNumber	

Devuelve el numero del ultimo comprobante impreso segun tipo de factura como parámetro hay que pasarle una variable estatica "tipo_cbte"

### Ultimo numero de Ticket A
{
	"getLastNumber": "TA"
}

### Ultimo numero de Factura A
{
	"getLastNumber": "FA"
}

### Ultimo numero de Nota de Credito A
{
	"getLastNumber": "NCA"
}


## RESPUESTA 
La respuesta es un JSON con el siguiente formato
	
### En el caso del printTicket la rta es el numero impreso
{
	"rta": [{
		"action": "printTicket",
		"rta": "7"
	}]
}

### En el caso del CierreZ el objeto rta es similar a este
	HASAR:
	{ 
	  _RESERVADO_SIEMPRE_CERO_: "0" (Integer)  
	  _cant_doc_fiscales_: "0" (Integer)  
	  _cant_doc_fiscales_a_emitidos_: "0" (Integer)  
	  _cant_doc_fiscales_bc_emitidos_: "0" (Integer)  
	  _cant_doc_fiscales_cancelados_: "0" (Integer)  
	  _cant_doc_nofiscales_: "1" (Integer)  
	  _cant_doc_nofiscales_homologados_: "0" (Integer)  
	  _cant_nc_a_fiscales_a_emitidos_: "0" (Integer)  
	  _cant_nc_bc_emitidos_: "0" (Integer)  
	  _cant_nc_canceladas_: "0" (Integer)  
	  _monto_credito_nc_: "0.00" (Float)  
	  _monto_imp_internos_: "0.00" (Float)  
	  _monto_imp_internos_nc_: "0.00" (Float)  
	  _monto_iva_doc_fiscal_: "0.00" (Float)  
	  _monto_iva_nc_: "0.00" (Float)  
	  _monto_iva_no_inscripto_: "0.00" (Float)  
	  _monto_iva_no_inscripto_nc_: "0.00" (Float)  
	  _monto_percepciones_: "0.00" (Float)  
	  _monto_percepciones_nc_: "0.00" (Float)  
	  _monto_ventas_doc_fiscal_: "0.00"(Float)  
	  _status_fiscal_: "0600" (Hexadecimal)  
	  _status_impresora_: "C080" (Hexadecimal)  
	  _ultima_nc_a_: "30" (Integer)  
	  _ultima_nc_b_: "327" (Integer)  
	  _ultimo_doc_a_: "2262" (Integer)  
	  _ultimo_doc_b_: "66733" (Integer)  
	  _ultimo_remito_: "0" (Integer)  
	  _zeta_numero_: "1" (Integer)  
	}  
	
	EPSON:
	{
		_status_fiscal_: "0600" (Hexadecimal)  
		_status_impresora_: "C080" (Hexadecimal)  
		_zeta_numero_: "1" (Integer)  
		_cant_doc_fiscales_cancelados_: "0" (Integer)  
		_cant_doc_nofiscales_homologados_: "0" (Integer) 
		_cant_doc_nofiscales_: "1" (Integer)
		_cant_doc_fiscales_bc_emitidos_: "0" (Integer)  
		_cant_doc_fiscales_a_emitidos_: "0" (Integer)  
		_ultimo_doc_bc_: "66733" (Integer)  
		_monto_ventas_doc_fiscal_: "0.00"(Float) 
		_monto_iva_doc_fiscal_: "0.00" (Float)  
		_monto_percepciones_: "0.00" (Float) 
		_ultimo_doc_a_: "2262" (Integer)
     }

### Tambien puede venir un error en la impresion o configuracion

{
	"err": "Por el momento, la impresion de remitos no esta habilitada."
}

### O puede venir un mensaje de la impresora

{
	"msg": ["Poco papel para comprobantes o tickets"]
}
