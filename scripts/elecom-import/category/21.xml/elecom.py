#!/usr/bin/python
file = '21.xml'
import urllib2
import codecs
import xml.dom.minidom
document = open(file).read()

dom = xml.dom.minidom.parseString(document)

def getText(nodelist):
  rc = []
  for node in nodelist:
    if node.nodeType == node.TEXT_NODE:
      rc.append(node.data.encode("utf-8"))
  return ''.join(rc)

def handleProductshow(productshow):
  print 'productname,price,color,warrenty,details'
  products = productshow.getElementsByTagName("PRODUCT")
  handleProducts(products)

def handleProducts(products):
  for product in products:
    handleProduct(product)

def handleProduct(product):
  pname = product.getElementsByTagName("PRODUCT_NAME")
  if pname:handleCSV(pname[0])
  pprice = product.getElementsByTagName("PRICE")
  if pprice:handleCSV(pprice[0])
  purl = product.getElementsByTagName("PRODUCT_URL")
  if purl:handleCSV(purl[0])
  pimage = product.getElementsByTagName("IMAGE")
  if pimage:handleCSV(pimage[0])
  pmanuf = product.getElementsByTagName("MANUFACTURER")
  if pmanuf:handleCSV(pmanuf[0])
  print 

def handleCSV(item):
  print "%s," % getText(item.childNodes),;


handleProductshow(dom)

