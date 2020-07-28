package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"math/rand"
	"net"
	"sync"
	"time"
)

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
	seq    uint32
	ack    uint32
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
	binary.LittleEndian.PutUint32(b[4:8], h.seq)
	binary.LittleEndian.PutUint32(b[8:12], h.ack)
	return nil
}

// Populate a slitheen block struct with bytes b
func (h *socksBlock) Unmarshal(b []byte) error {
	if len(b) < 4 {
		return fmt.Errorf("Not enough bytes to parse block")
	}

	h = new(socksBlock)
	h.stream = binary.LittleEndian.Uint16(b[0:2])
	h.length = binary.LittleEndian.Uint16(b[2:4])
	h.seq = binary.LittleEndian.Uint32(b[4:8])
	h.ack = binary.LittleEndian.Uint32(b[8:12])

	return nil
}

func NewServer() *Server {
	pr, pw := io.Pipe()
	return &Server{
		streams: make(map[uint16]*SocksConn),
		pr:      pr,
		pw:      pw,
	}
}

func (s *Server) ListenAndServe(addr string) error {
	l, err := net.Listen("tcp", addr)
	log.Printf("Listening on %s", addr)

	if err != nil {
		log.Printf("Error setting up socks listener: %s", err.Error())
		return err
	}

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
			s.serveConn(sconn)
		}()
	}
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
	methods := make([]byte, 1)
	if _, err := io.ReadFull(conn, methods[:]); err != nil {
		return err
	}

	var responded bool
	for i := 0; i < int(uint8(methods[0])); i++ {
		if _, err := conn.Write([]byte{0x05, 0x00}); err != nil {
			return err
		}
		responded = true
	}

	if !responded {
		if _, err := conn.Write([]byte{0x05, 0xFF}); err != nil {
			return err
		}
	}

	// Accept connect request
	if _, err := conn.Write([]byte{0x05, 0x00, 0x00, 0x01}); err != nil {
		return err
	}

	return nil
}

// Sends upstream data from the socks connection to the multiplexer, and
// return demultiplexed downstream data to the socks connection
func (s *Server) serveConn(conn *SocksConn) {

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

	for { //TODO: this probably needs to be fixed
		//TODO: read from s.pr
		bytes, err := ioutil.ReadAll(&io.LimitedReader{R: s.pr, N: 65535})
		if err != nil {
			return
		}
		n, err := conn.Write(bytes)
		if err != nil {
			return //TODO: fix
		}
		log.Printf("Wrote %d bytes to OUS", n)
		if n < len(bytes) {
			println("Error: short write") //TODO: figure out how to handle this
		}

	}
}

// reads data from OUS connection and writes to socks channel
func (s *Server) demultiplex(conn net.Conn) {
	defer conn.Close()

	for {
		var block *socksBlock
		infoBytes := make([]byte, 4, 4)
		n, err := conn.Read(infoBytes)
		if err != nil {
			return
		}
		if n < 4 {
			println("Error: short read")
		}
		block.Unmarshal(infoBytes)

		bytes := make([]byte, block.length, block.length)
		n, err = conn.Read(bytes)
		if err != nil {
			return
		}
		if uint16(n) < block.length {
			println("Error: short read")
		}
		s.streams[block.stream].pw.Write(bytes)
	}
}

func main() {
	println("Hello.")

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

	socksServer.ListenAndServe(":1080")
}
