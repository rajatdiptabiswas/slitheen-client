package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"io/ioutil"
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
        seq uint32
        ack uint32
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

	if err != nil {
		return err
	}

	return s.Serve(l)
}

func (s *Server) Serve(l net.Listener) error {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))
	for {
		conn, err := l.Accept()
		if err != nil {
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
			s.serveConn(sconn)
		}()
	}
}

// Sends upstream data from the socks connection to the multiplexer, and
// return demultiplexed downstream data to the socks connection
func (s *Server) serveConn(conn *SocksConn) {

	var wg sync.WaitGroup
	copier := func(dst io.WriteCloser, src io.ReadCloser) {
		defer wg.Done()
		io.Copy(dst, src)
		dst.Close()
		src.Close()
	}
	wg.Add(2)
	go copier(conn, conn.pr)
	go copier(s.pw, conn)
	wg.Wait()
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

	socksServer.ListenAndServe("127.0.0.1:1080")
}
