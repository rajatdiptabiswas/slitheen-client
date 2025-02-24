package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"math/rand"
	"net"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"time"
)

var sigChan chan os.Signal

type Server struct {
	// map of stream IDs to SOCKS connections
	streams map[uint16]*SocksConn
	pr      *io.PipeReader
	pw      *io.PipeWriter
}

type SocksConn struct {
	conn   net.Conn
	stream uint16
	pr     *io.PipeReader
	pw     *io.PipeWriter
}

// put into socksBlock structure
func (c *SocksConn) Read(b []byte) (int, error) {
	if len(b) < 4 {
		return 0, fmt.Errorf("Buffer not big enough for whole block")
	}
	n, err := c.conn.Read(b[4:])
	if err != nil {
		return n, err
	}

	block := new(socksBlock)
	block.stream = c.stream
	block.length = uint16(n)
	err = block.Marshal(b)

	log.Printf("Bundled a block of size %d", n)
	return n + 4, nil
}

func (c *SocksConn) Write(b []byte) (int, error) {
	return c.conn.Write(b)
}

func (c *SocksConn) Close() error {
	return c.conn.Close()
}

type socksBlock struct {
	stream uint16
	length uint16
	data   []byte
}

// Convert a slitheen block into a byte stream to send to the OUS
func (h *socksBlock) Marshal(b []byte) error {
	if h == nil {
		return fmt.Errorf("nil socksBlock")
	}

	if len(b) < 4 {
		return fmt.Errorf("Not enough bytes to marshal block")
	}

	binary.LittleEndian.PutUint16(b[0:2], h.stream)
	binary.LittleEndian.PutUint16(b[2:4], h.length)
	return nil
}

// Populate a slitheen block struct with bytes b
func Unmarshal(b []byte) (*socksBlock, error) {
	if len(b) < 4 {
		return nil, fmt.Errorf("Not enough bytes to parse block")
	}

	h := new(socksBlock)
	h.stream = binary.LittleEndian.Uint16(b[0:2])
	h.length = binary.BigEndian.Uint16(b[2:4])

	return h, nil
}

func NewServer() *Server {
	pr, pw := io.Pipe()
	return &Server{
		streams: make(map[uint16]*SocksConn),
		pr:      pr,
		pw:      pw,
	}
}

func (s *Server) socksAcceptLoop(l net.Listener) error {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))
	for {
		conn, err := l.Accept()
		if err != nil {
			log.Printf("Error accepting new connection: %s", err.Error())
			return err
		}

		go func() {
			stream := uint16(r.Uint32())
			pr, pw := io.Pipe()
			sconn := &SocksConn{
				conn:   conn,
				stream: stream,
				pr:     pr,
				pw:     pw,
			}

			s.streams[stream] = sconn
			if err := establishSOCKSConnection(conn); err != nil {
				log.Printf("Error establishing SOCKS connection: %s", err.Error())
				conn.Close()
				return
			}
			request, err := readRequest(conn)
			if err != nil {
				log.Printf("Error reading in new connect request: %s", err.Error())
			}
			s.serveConn(request, sconn)
		}()
	}
}

func (s *Server) ListenAndServe(addr string) (net.Listener, error) {
	l, err := net.Listen("tcp", addr)
	log.Printf("Listening on %s", addr)

	if err != nil {
		log.Printf("Error setting up socks listener: %s", err.Error())
		return nil, err
	}

	go s.socksAcceptLoop(l)

	return l, nil

}

func readRequest(conn net.Conn) ([]byte, error) {
	request := make([]byte, 3)

	// Read in request header
	if _, err := io.ReadFull(conn, request[:]); err != nil {
		return nil, err
	}

	if request[0] != 0x05 {
		return nil, fmt.Errorf("invalid SOCKS version")
	}

	// Read in address
	addrType := make([]byte, 1)
	if _, err := io.ReadFull(conn, addrType[:]); err != nil {
		return nil, err
	}
	request = append(request, addrType...)

	switch addrType[0] {
	case 0x01:
		// IPv4
		addr := make([]byte, 4)
		if _, err := io.ReadFull(conn, addr[:]); err != nil {
			return nil, err
		}
		request = append(request, addr...)
	case 0x03:
		// Domain name
		addrLen := make([]byte, 1)
		if _, err := io.ReadFull(conn, addrLen[:]); err != nil {
			return nil, err
		}
		request = append(request, addrLen...)
		addr := make([]byte, int(addrLen[0]))
		if _, err := io.ReadFull(conn, addr[:]); err != nil {
			return nil, err
		}
		request = append(request, addr...)
	case 0x04:
		// IPv6
		addr := make([]byte, 16)
		if _, err := io.ReadFull(conn, addr[:]); err != nil {
			return nil, err
		}
		request = append(request, addr...)
	default:
		return nil, fmt.Errorf("unrecognized address type: 0x%x", addrType[0])
	}

	port := make([]byte, 2)
	if _, err := io.ReadFull(conn, port[:]); err != nil {
		return nil, err
	}
	request = append(request, port...)

	// Accept connect request
	response := make([]byte, 10, 10)
	response[0] = 0x05
	response[3] = 0x01
	if _, err := conn.Write(response); err != nil {
		log.Printf("Error accepting connect request: %s", err.Error())
	}

	return request, nil

}

func establishSOCKSConnection(conn net.Conn) error {

	// Check SOCKS version
	version := make([]byte, 1)
	if _, err := io.ReadFull(conn, version[:]); err != nil {
		return err
	}

	if version[0] != uint8(5) {
		return fmt.Errorf("Received unsupported SOCKS version %d", version[0])
	}

	// Check methods
	numMethods := make([]byte, 1)
	if _, err := io.ReadFull(conn, numMethods[:]); err != nil {
		return err
	}
	methods := make([]byte, int(numMethods[0]))
	if _, err := io.ReadFull(conn, methods[:]); err != nil {
		return err
	}

	var responded bool
	for _, method := range methods {
		if method == 0x00 {
			if _, err := conn.Write([]byte{0x05, 0x00}); err != nil {
				return err
			}
			responded = true
			break
		}
	}

	if !responded {
		if _, err := conn.Write([]byte{0x05, 0xFF}); err != nil {
			return err
		}
	}

	return nil
}

// Sends upstream data from the socks connection to the multiplexer, and
// return demultiplexed downstream data to the socks connection
func (s *Server) serveConn(request []byte, conn *SocksConn) {

	// Send request to OUS
	b := make([]byte, 4)
	block := new(socksBlock)
	block.stream = conn.stream
	block.length = uint16(len(request))
	block.Marshal(b)
	if _, err := s.pw.Write(append(b, request...)); err != nil {
		log.Printf("Error relaying connect request: %s", err.Error())
	}

	var wg sync.WaitGroup
	log.Printf("Started copy loop for stream %d", conn.stream)
	copier := func(dst io.WriteCloser, src io.ReadCloser) {
		defer wg.Done()
		io.Copy(dst, src)
		conn.Close()
		conn.pr.Close()
	}
	wg.Add(2)
	go copier(conn, conn.pr)
	go copier(s.pw, conn)
	wg.Wait()
	log.Printf("Stopped copy loop for stream %d", conn.stream)
}

// reads SOCKS data from a channel and writes to OUS
func (s *Server) multiplex(conn net.Conn) {
	defer s.pr.Close()
	defer conn.Close()

	if _, err := io.Copy(conn, s.pr); err != nil {
		log.Printf("Error copying from multiplex pipe to OUS: %s", err.Error())
	}
	sigChan <- syscall.SIGTERM

}

// reads data from OUS connection and writes to socks channel
func (s *Server) demultiplex(conn net.Conn) {
	defer conn.Close()
	defer s.pr.Close()
	defer s.pw.Close()

	for {
		infoBytes := make([]byte, 4)
		n, err := io.ReadFull(conn, infoBytes[:])
		if err != nil {
			log.Printf("Error reading downstream data: %s", err.Error())
			break
		}
		if n < 4 {
			log.Printf("Error: short read")
		}
		block, err := Unmarshal(infoBytes)
		if err != nil {
			break
		}
		log.Printf("Received %d bytes for stream %d", block.length, block.stream)

		bytes := make([]byte, block.length, block.length)
		n, err = conn.Read(bytes)
		if err != nil {
			break
		}
		if uint16(n) < block.length {
			log.Printf("Error: short read")
		}

		stream, ok := s.streams[block.stream]
		if !ok {
			log.Printf("Error: couldn't find stream %d", block.stream)
		} else {
			stream.pw.Write(bytes)
		}
	}
	sigChan <- syscall.SIGTERM
}

func main() {
	println("Hello.")

	sigChan = make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGTERM)

	//open a connection to the OUS
	conn, err := net.Dial("tcp", "127.0.0.1:57173")
	if err != nil {
		return //TODO: handle returns
	}

	socksServer := NewServer()
	// go routine that multiplexes SOCKS connections
	go socksServer.multiplex(conn)

	// go routine that demultiplexes SOCKS connections
	go socksServer.demultiplex(conn)

	l, err := socksServer.ListenAndServe(":1080")
	if err != nil {
		log.Printf("Error connecting to OUS: %s", err.Error())
		return
	}

	<-sigChan
	l.Close()
	conn.Close()

}
