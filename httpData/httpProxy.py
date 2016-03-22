# -*- coding: utf-8 -*-
import sys
import os
import socket
import ssl
import select
import httplib
import urlparse
import threading
import gzip
import zlib
import time
import json
import re
from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler
from SocketServer import ThreadingMixIn
from cStringIO import StringIO
from subprocess import Popen, PIPE
from HTMLParser import HTMLParser

#filename = 'proxyOutput-%s'%datetime.now().strftime('%Y-%m-%d')
f1 = open('mainData','w')

f = open('extraData', 'w')




def with_color(c, s):
    #print ("def #1")
    return "\x1b[%dm%s\x1b[0m" % (c, s)


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    address_family = socket.AF_INET6
    daemon_threads = True

    def handle_error(self, request, client_address):
        # surpress socket/ssl related errors
        #print ("def #2")
        cls, e = sys.exc_info()[:2]
        if cls is socket.error or cls is ssl.SSLError:
            pass
        else:
            return HTTPServer.handle_error(self, request, client_address)


class ProxyRequestHandler(BaseHTTPRequestHandler):
    cakey = 'ca.key'
    cacert = 'ca.crt'
    certkey = 'cert.key'
    certdir = 'certs/'
    timeout = 5
    lock = threading.Lock()

    def __init__(self, *args, **kwargs):
        #print ("def #3")
        self.tls = threading.local()
        self.tls.conns = {}

        BaseHTTPRequestHandler.__init__(self, *args, **kwargs)

    def log_error(self, format, *args):
        #print ("def #4")
        # surpress "Request timed out: timeout('timed out',)"
        if isinstance(args[0], socket.timeout):
            return

        self.log_message(format, *args)

    def do_CONNECT(self):
        #print ("def #5")
        if os.path.isfile(self.cakey) and os.path.isfile(self.cacert) and os.path.isfile(self.certkey) and os.path.isdir(self.certdir):
            self.connect_intercept()
        else:
            self.connect_relay()

    def connect_intercept(self):
        #print ("def #6")
        hostname = self.path.split(':')[0]

        certpath = "%s/%s.crt" % (self.certdir.rstrip('/'), hostname)

        with self.lock:
            if not os.path.isfile(certpath):
                epoch = "%d" % (time.time() * 1000)
                p1 = Popen(["openssl", "req", "-new", "-key", self.certkey, "-subj", "/CN=%s" % hostname], stdout=PIPE)
                p2 = Popen(["openssl", "x509", "-req", "-days", "3650", "-CA", self.cacert, "-CAkey", self.cakey, "-set_serial", epoch, "-out", certpath], stdin=p1.stdout, stderr=PIPE)
                p2.communicate()

        self.wfile.write("%s %d %s\r\n" % (self.protocol_version, 200, 'Connection Established'))
        self.end_headers()

        self.connection = ssl.wrap_socket(self.connection, keyfile=self.certkey, certfile=certpath, server_side=True)
        self.rfile = self.connection.makefile("rb", self.rbufsize)
        self.wfile = self.connection.makefile("wb", self.wbufsize)

        conntype = self.headers.get('Proxy-Connection', '')
        if conntype.lower() == 'close':
            self.close_connection = 1
        elif (conntype.lower() == 'keep-alive' and self.protocol_version >= "HTTP/1.1"):
            self.close_connection = 0

    def connect_relay(self):
        #print ("def #7")
        address = self.path.split(':', 1)
        
        address[1] = int(address[1]) or 443
        try:
            s = socket.create_connection(address, timeout=self.timeout)
        except Exception as e:
            self.send_error(502)
            return
        self.send_response(200, 'Connection Established')
        self.end_headers()

        conns = [self.connection, s]
        self.close_connection = 0
        while not self.close_connection:
            rlist, wlist, xlist = select.select(conns, [], conns, self.timeout)
            if xlist or not rlist:
                break
            for r in rlist:
                other = conns[1] if r is conns[0] else conns[0]
                data = r.recv(8192)
                if not data:
                    self.close_connection = 1
                    break
                other.sendall(data)

    def do_GET(self):
        #print ("def #8")
      
        if self.path == 'http://proxy2.test/':
            self.send_cacert()
            return
        #print ("mainhost: %s \n" % (self.headers['Host']))
        
        ######## request ########
        req = self
        content_length = int(req.headers.get('Content-Length', 0))
        req_body = self.rfile.read(content_length) if content_length else None

        if req.path[0] == '/':
            if isinstance(self.connection, ssl.SSLSocket):
                req.path = "https://%s%s" % (req.headers['Host'], req.path)
            else:
                req.path = "http://%s%s" % (req.headers['Host'], req.path)

    
      ########### request host name#########
        print ("Working on host: %s \n" % (req.headers['Host']))
       
        req_body_modified = self.request_handler(req, req_body)
        if req_body_modified is not None:
            req_body = req_body_modified
            req.headers['Content-length'] = str(len(req_body))
            
      
         #   print ("request size: %s \n" % req.headers.get('content-length')) ###########content size#########
         #   print ("request type: %s \n" % req.headers.get('content-type'))   ###########content type#########

        
        u = urlparse.urlsplit(req.path)
        scheme, netloc, path = u.scheme, u.netloc, (u.path + '?' + u.query if u.query else u.path)
        assert scheme in ('http', 'https')
        if netloc:
            req.headers['Host'] = netloc
        req_headers = self.filter_headers(req.headers)

        try:
            origin = (scheme, netloc)
            if not origin in self.tls.conns:
                if scheme == 'https':
                    self.tls.conns[origin] = httplib.HTTPSConnection(netloc, timeout=self.timeout)
                else:
                    self.tls.conns[origin] = httplib.HTTPConnection(netloc, timeout=self.timeout)
            conn = self.tls.conns[origin]
            conn.request(self.command, path, req_body, dict(req_headers))
    
        ####### response###########
            res = conn.getresponse()
            res_body = res.read()
        except Exception as e:
            if origin in self.tls.conns:
                del self.tls.conns[origin]
            self.send_error(502)
            return
        

        version_table = {10: 'HTTP/1.0', 11: 'HTTP/1.1'}
        setattr(res, 'headers', res.msg)
        setattr(res, 'response_version', version_table[res.version])

        content_encoding = res.headers.get('Content-Encoding', 'identity')
        res_body_plain = self.decode_content_body(res_body, content_encoding)

        res_body_modified = self.response_handler(req, req_body, res, res_body_plain)
        if res_body_modified is not None:
            res_body_plain = res_body_modified
            res_body = self.encode_content_body(res_body_plain, content_encoding)
            res.headers['Content-Length'] = str(len(res_body))

  
   ###########content size and type#########
  #      print (" response size: %s \n" % res.headers.get('content-length'))
  #      print (" response type: %s \n" % res.headers.get('content-type'))


        res_headers = self.filter_headers(res.headers)

        self.wfile.write("%s %d %s\r\n" % (self.protocol_version, res.status, res.reason))
        for line in res_headers.headers:
            self.wfile.write(line)
        self.end_headers()
        self.wfile.write(res_body)
        self.wfile.flush()

        with self.lock:
            self.save_handler(req, req_body, res, res_body_plain)

    do_HEAD = do_GET
    do_POST = do_GET
    do_OPTIONS = do_GET

    def filter_headers(self, headers):
        #print ("def #9")
        # http://tools.ietf.org/html/rfc2616#section-13.5.1
        hop_by_hop = ('connection', 'keep-alive', 'proxy-authenticate', 'proxy-authorization', 'te', 'trailers', 'transfer-encoding', 'upgrade')
        for k in hop_by_hop:
            del headers[k]
        return headers

    def encode_content_body(self, text, encoding):
        #print ("def #10")
        if encoding == 'identity':
            data = text
        elif encoding in ('gzip', 'x-gzip'):
            io = StringIO()
            with gzip.GzipFile(fileobj=io, mode='wb') as f:
                f.write(text)
            data = io.getvalue()
        elif encoding == 'deflate':
            data = zlib.compress(text)
        else:
            raise Exception("Unknown Content-Encoding: %s" % encoding)
        return data

    def decode_content_body(self, data, encoding):
        #print ("def #11")  
        if encoding == 'identity':
            text = data
        elif encoding in ('gzip', 'x-gzip'):
            io = StringIO(data)
            with gzip.GzipFile(fileobj=io) as f:
                text = f.read()
        elif encoding == 'deflate':
            try:
                text = zlib.decompress(data)
            except zlib.error:
                text = zlib.decompress(data, -zlib.MAX_WBITS)
        else:
            raise Exception("Unknown Content-Encoding: %s" % encoding)
        return text

    def send_cacert(self):
        #print ("def #12")
        with open(self.cacert, 'rb') as f:
            data = f.read()

        self.wfile.write("%s %d %s\r\n" % (self.protocol_version, 200, 'OK'))
        self.send_header('Content-Type', 'application/x-x509-ca-cert')
        self.send_header('Content-Length', len(data))
        self.send_header('Connection', 'close')
        self.end_headers()
        self.wfile.write(data)

    def print_info(self, req, req_body, res, res_body):
        #print ("def #13")
        def parse_qsl(s):
            #print ("def #14")
            return '\n'.join("%-20s %s" % (k, v) for k, v in urlparse.parse_qsl(s, keep_blank_values=True))

      
        request_data = "%s\t%s\t%s\t%s" % (req.headers['Referer'],req.headers['Host'],req.command,req.path)
        f1.write('Request:\t')
        f1.write(str(request_data))
        f1.write(str('\n'))
        
        response_data = "%s\t%s" % (res.headers.get('content-length'),res.headers.get('content-type'))
        f1.write('Response:\t')
        f1.write(str(response_data))
        f1.write(str('\n'))
        
    #    print (" request host: %s \n" % (req.headers['Host']))
    #    print (" request referer: %s \n" % (req.headers['Referer']))
    #    print (" response size: %s \n" % res.headers.get('content-length'))
    #    print (" response type: %s \n" % res.headers.get('content-type'))
    #    print (" req path: %s \n" % req.path)
    #    print (" req command: %s \n" % req.command)        

        req_header_text = "%s %s %s\n%s" % (req.command, req.path, req.request_version, req.headers)
        res_header_text = "%s %d %s\n%s" % (res.response_version, res.status, res.reason, res.headers)
        
     #   print ("\nRequest:\t %s \t %s" % (req.headers['Host']), req.command)
     #   print ("\nResponse:\t %s \t %d" % (res.headers['Content-Type']), int(res.headers['Content-Length']))
        print ("\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n")
        
   #     print ("=====OUTPUT 1=====")
   #     req_header_text = " %s \n" % ( req.host)
   #     res_header_text = "%s %d %s\n%s" % (res.response_version, res.status, res.reason, res.headers)
        
    #    r1 = conn.getresponse()
    #    data1 = r1.read()
    #    print ("+++++MY OUTPUT+++++")
    #    print (data)

     #   print with_color(33, req_header_text)
        f.write(str(req_header_text))

        u = urlparse.urlsplit(req.path)
        if u.query:
            query_text = parse_qsl(u.query)
       #     print with_color(32, "==== QUERY PARAMETERS ====\n%s\n" % query_text)
            f.write (str('==== QUERY PARAMETERS ====\n'))
            f.write (str(query_text))
            f.write (str('\n'))

        cookie = req.headers.get('Cookie', '')
        if cookie:
            cookie = parse_qsl(re.sub(r';\s*', '&', cookie))
      #      print with_color(32, "==== COOKIE ====\n%s\n" % cookie)
            f.write (str('==== COOKIE ====\n'))
            f.write (str(cookie))
            f.write (str('\n'))


        auth = req.headers.get('Authorization', '')
        if auth.lower().startswith('basic'):
            token = auth.split()[1].decode('base64')
      #      print with_color(31, "==== BASIC AUTH ====\n%s\n" % token)
            f.write (str('==== BASIC AUTH ====\n'))
            f.write (str(token))
            f.write (str('\n'))

        if req_body is not None:
            req_body_text = None
            content_type = req.headers.get('Content-Type', '')

            if content_type.startswith('application/x-www-form-urlencoded'):
                req_body_text = parse_qsl(req_body)
            elif content_type.startswith('application/json'):
                try:
                    json_obj = json.loads(req_body)
                    json_str = json.dumps(json_obj, indent=2)
                    if json_str.count('\n') < 50:
                        req_body_text = json_str
                    else:
                        lines = json_str.splitlines()
                        req_body_text = "%s\n(%d lines)" % ('\n'.join(lines[:50]), len(lines))
                except ValueError:
                    req_body_text = req_body
            elif len(req_body) < 1024:
                req_body_text = req_body

            if req_body_text:
                #print with_color(32, "==== REQUEST BODY ====\n%s\n" % req_body_text)
                f.write (str('==== REQUEST BODY ====\n'))
                f.write (str(req_body_text))
                f.write (str('\n'))

        print with_color(36, res_header_text)
        f.write(str(res_header_text))

        cookies = res.headers.getheaders('Set-Cookie')
        if cookies:
            cookies = '\n'.join(cookies)
         #   print with_color(31, "==== SET-COOKIE ====\n%s\n" % cookies)
            f.write (str('==== SET-COOKIE ====\n'))
            f.write (str(cookies))
            f.write (str('\n'))

        if res_body is not None:
            res_body_text = None
            content_type = res.headers.get('Content-Type', '')

            if content_type.startswith('application/json'):
                try:
                    json_obj = json.loads(res_body)
                    json_str = json.dumps(json_obj, indent=2)
                    if json_str.count('\n') < 50:
                        res_body_text = json_str
                    else:
                        lines = json_str.splitlines()
                        res_body_text = "%s\n(%d lines)" % ('\n'.join(lines[:50]), len(lines))
                except ValueError:
                    res_body_text = res_body
            elif content_type.startswith('text/html'):
                m = re.search(r'<title[^>]*>\s*([^<]+?)\s*</title>', res_body, re.I)
                if m:
                    h = HTMLParser()
                    #print with_color(32, "==== HTML TITLE ====\n%s\n" % h.unescape(m.group(1).decode('utf-8')))
                    f.write (str('==== HTML TITLE ====\n'))
                    f.write (str(h.unescape(m.group(1).decode('utf-8'))))
                    f.write (str('\n'))
                
            elif content_type.startswith('text/') and len(res_body) < 1024:
                res_body_text = res_body

            if res_body_text:
              #  print with_color(32, "==== RESPONSE BODY ====\n%s\n" % res_body_text)
                f.write (str('==== RESPONSE BODY ====\n'))
                f.write (str(res_body_text))
                f.write (str('\n'))
                
    def request_handler(self, req, req_body):
        #print ("def #15")
        pass

    def response_handler(self, req, req_body, res, res_body):
        #print ("def #16")
        pass

    def save_handler(self, req, req_body, res, res_body):
        #print ("def #17")
        self.print_info(req, req_body, res, res_body)
        
  #      req_host = req.headers(['Host'])
  #      req_length = int(len(req_body))

#        res_host = res.headers(['Host'])
#        res_length = int(len(res_body))

 #       print ("===DATA===\n %s\t %d\n %s\t %d\n" % req_host, req_length, res_host, res_length  )
        
def test(HandlerClass=ProxyRequestHandler, ServerClass=ThreadingHTTPServer, protocol="HTTP/1.1"):
    #print ("def #18")

    if sys.argv[1:]:
        port = int(sys.argv[1])
    else:
        port = 8080
    server_address = ('', port)

    HandlerClass.protocol_version = protocol
    httpd = ServerClass(server_address, HandlerClass)

    sa = httpd.socket.getsockname()
    print "Serving HTTP Proxy on", sa[0], "port", sa[1], "..."
    httpd.serve_forever()


if __name__ == '__main__':
    test()


