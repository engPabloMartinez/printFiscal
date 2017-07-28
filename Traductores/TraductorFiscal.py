# -*- coding: utf-8 -*-
from Traductores.TraductorInterface import TraductorInterface
from ComandoInterface import ValidationError

class TraductorFiscal(TraductorInterface):
	
	# tipo de impresion, F: Fiscal (para factura/nd/nc), R: Remito
	_tipoImpresion= None

	def dailyClose(self, type):
		"Comando X o Z"
		# cancelar y volver a un estado conocido
		self.comando.cancelAnyDocument()
		
		ret = self.comando.dailyClose(type)
		return ret



	def setHeader(self, *args):
		"SetHeader"
		ret = self.comando.setHeader(list(args))
		return ret

	def setTrailer(self, *args):
		"SetTrailer"
		ret = self.comando.setTrailer(list(args))
		return ret


	def openDrawer(self, *args):
		"Abrir caja registradora"
		return self.comando.openDrawer()


	def getLastNumber(self, tipo_cbte):
		"Devuelve el último número de comprobante"
		
		letra_cbte = tipo_cbte[-1] if len(tipo_cbte) > 1 else None
		
		if tipo_cbte.startswith('R'):
			return self.comando.getLastRemitNumber()
		elif tipo_cbte.startswith("NC"):	
			return self.comando.getLastCreditNoteNumber(letra_cbte)
		else: 
			return self.comando.getLastNumber(letra_cbte)

	def cancelDocument(self):
		"Cancelar comprobante en curso"
		return self.comando.cancelDocument()


	def printTicket(self, cabecera=None, items=[], formasPago=[], dtosGenerales=[], percepciones=[], descuentosRecargos=[], encabezado=None, pie=None):
		self._tipoImpresion = "F"
		
		# cancelar y volver a un estado conocido
		self.comando.cancelAnyDocument()
		
		# Seteo el encabezado
		if encabezado:
			self.setHeader( *encabezado )

		#seteo el pie	
		if pie:
			self.setTrailer( *pie )

		# creo la cabecera del comprobante fiscal
		if cabecera:
			self._abrirComprobante(**cabecera)
		else:
			self._abrirComprobante()
		
		# cargo los items
		for item in items:
			self._imprimirItem(**item)

		# descuentos y/o recargos	
		if descuentosRecargos:
			for opc in descuentosRecargos:
				self._imprimirDtoRec(**opc)
		
		# percepciones
		if percepciones:
			for percepcion in percepciones:
				self._imprimirPercepcion(**percepcion)
		
		# descuentos generales a nivel de todo el comprobante	
		if dtosGenerales:
			for dtoGral in dtosGenerales:
				self._imprimirDtosGenerales(**dtoGral)
		
		# formas de pago
		if formasPago:
			for formaPago in formasPago:
				self._imprimirFormasPago(**formaPago)
				
		rta = self._cerrarComprobante()
		return rta
	
	def printRemito(self, cabecera=None, items=[]):
		self._tipoImpresion = "R"
		
		# cancelar y volver a un estado conocido
		self.comando.cancelAnyDocument()
		
		if self.comando.DEFAULT_DRIVER == "Hasar":
			# creo la cabecera de remito
			self._abrirComprobante(**cabecera)

			#Items del remito
			for item in items:
				self._imprimirItem(**item)

			rta = self._cerrarComprobante()
			return rta
		else:
			raise ValidationError("Por el momento, la impresion de remitos no esta habilitada.")
			
	def _abrirComprobante(self, 
						 tipo_cbte="T", 							# tique
						 tipo_responsable="CONSUMIDOR_FINAL",
						 tipo_doc="SIN_CALIFICADOR", nro_doc=" ",     # sin especificar
						 nombre_cliente=" ", domicilio_cliente=" ",
						 referencia=None,                           # comprobante original (ND/NC)
						 copias=1,
						 **kwargs
						 ):
		"Creo un objeto factura/remito (internamente) e imprime el encabezado"
		# crear la estructura interna
		
		#remito
		if self._tipoImpresion == 'R': 
			self.remito = {"encabezado": dict(tipo_responsable=tipo_responsable,
											   tipo_doc=tipo_doc, nro_doc=nro_doc,
											   nombre_cliente=nombre_cliente, 
											   domicilio_cliente=domicilio_cliente,
											   copias=copias),
								 "items": []}
		#factura, nc, nd
		else: 
			self.factura = {"encabezado": dict(tipo_cbte=tipo_cbte,
											   tipo_responsable=tipo_responsable,
											   tipo_doc=tipo_doc, nro_doc=nro_doc,
											   nombre_cliente=nombre_cliente, 
											   domicilio_cliente=domicilio_cliente,
											   referencia=referencia),
							"items": [], "formasPago": [], "dtosGenerales": [], "percepciones": [], "descuentosRecargos": []}
			
		printer = self.comando

		letra_cbte = tipo_cbte[-1] if len(tipo_cbte) > 1 else None
		
		# mapear el tipo de cliente (posicion/categoria)
		pos_fiscal = printer.ivaTypes.get(tipo_responsable)
		
		# mapear el numero de documento según RG1361
		doc_fiscal = printer.docTypes.get(tipo_doc)
		
		ret = False
		# enviar los comandos de apertura de comprobante fiscal:
		#ticket
		if tipo_cbte.startswith('T'):
			if letra_cbte:
				ret = printer.openTicket(letra_cbte)
			else:
				ret = printer.openTicket()
		#factura
		elif tipo_cbte.startswith("F"):
			ret = printer.openBillTicket(letra_cbte, nombre_cliente, domicilio_cliente, 
										 nro_doc, doc_fiscal, pos_fiscal)
		#nota de debito
		elif tipo_cbte.startswith("ND"):
			ret = printer.openDebitNoteTicket(letra_cbte, nombre_cliente, 
											  domicilio_cliente, nro_doc, doc_fiscal, 
											  pos_fiscal)
		#nota de credito
		elif tipo_cbte.startswith("NC"):
			ret = printer.openBillCreditTicket(letra_cbte, nombre_cliente, 
											   domicilio_cliente, nro_doc, doc_fiscal, 
											   pos_fiscal, referencia)
		#remito
		elif tipo_cbte.startswith("R"):
			ret = printer.openRemit(nombre_cliente, domicilio_cliente, nro_doc, doc_fiscal, pos_fiscal, copias)
		
		return ret

	def _imprimirItem(self, ds, qty, importe=0, alic_iva=21., tasaAjusteInternos=0, 
					  itemNegative=False, discount=0, discountDescription='', discountNegative=True):
		
		# documento fiscal
		if self._tipoImpresion == 'F': 
			"Envia un item (descripcion, cantidad, etc.) a una factura/nc/nd"
			self.factura["items"].append(dict(ds=ds, qty=qty, 
											  importe=importe, alic_iva=alic_iva, tasaAjusteInternos=tasaAjusteInternos,
											  itemNegative=itemNegative, discount=discount, discountDescription=discountDescription, discountNegative=discountNegative))
			if discountDescription == '':
				discountDescription = ds
			
			return self.comando.addItem(ds, float(qty), float(importe), float(alic_iva), float(tasaAjusteInternos)
										itemNegative, float(discount), discountDescription, discountNegative)
		# remito
		else: 
			"Envia un item (descripcion, cantidad, etc.) a un remito"
			self.remito["items"].append(dict(ds=ds, qty=qty))
			
			return self.comando.addRemitItem(ds, float(qty))								
 
	def _imprimirDtoRec(self, ds, importe, alic_iva=21., tasaAjusteInternos=0, itemNegative=True):
	
		self.factura["descuentosRecargos"].append(dict(ds=ds, importe=importe, alic_iva=alic_iva, tasaAjusteInternos=tasaAjusteInternos, 
										  itemNegative=itemNegative))
		return self.comando.addReturnRecharge(ds, float(importe), float(alic_iva), float(tasaAjusteInternos), 
											  itemNegative)
    
	def _imprimirDtosGenerales(self, ds, importe, alic_iva=21., negative=True):
	    self.factura["dtosGenerales"].append(dict(ds=ds, importe=importe, alic_iva=alic_iva, negative=negative))
	    return self.comando.addAdditional(ds, float(importe), float(alic_iva), negative)
	
	def _imprimirPercepcion(self, ds, importe, alic_iva='**.**', porcPerc=0):
		self.factura["percepciones"].append(dict(ds=ds, importe=importe, alic_iva=alic_iva, porcPerc=porcPerc))
		return self.comando.addPerception(ds, float(importe), alic_iva, porcPerc)
		
	def _imprimirFormasPago(self, ds, importe):
		"Imprime una linea con la forma de pago y monto"
		self.factura["formasPago"].append(dict(ds=ds, importe=importe))
		return self.comando.addPayment(ds, float(importe))	
		
	def _cerrarComprobante(self):
		"Envia el comando para cerrar un comprobante Fiscal"
		return self.comando.closeDocument()