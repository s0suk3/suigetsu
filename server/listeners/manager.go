package listeners

import (
	"fmt"
	"net/http"
	"sync"
	"time"

	"github.com/miekg/dns"
	"suigetsu/server/crypto"
	"suigetsu/server/queue"
	"suigetsu/server/receiver"
)

type Manager struct {
	key        [32]byte
	windowSize uint64
	activeHTTP *http.Server
	activeDNS  *dns.Server
	activeWS *http.Server
	mu         sync.Mutex
	Receiver   *receiver.Receiver
	Queue      *queue.Queue
}

func NewManager(key [32]byte, windowSize uint64, q *queue.Queue) *Manager {
	return &Manager{
		key:        key,
		windowSize: windowSize,
		Receiver:   receiver.NewReceiver(key),
		Queue:      q,
	}
}

func (m *Manager) Start() {
	for {
		counter := crypto.GetCounter(m.windowSize)
		protocol := crypto.GetProtocol(m.key, counter)
		fmt.Printf("active window counter: %d, protocol: %d\n", counter, protocol)
		switch protocol {
		case 0:
			m.StopDNS()
			m.StopWebSocket()
			m.StartHTTPS()
		case 1:
			m.StopHTTPS()
			m.StopWebSocket()
			m.StartDNS()
		case 2:
			m.StopHTTPS()
			m.StopDNS()
			m.StartWebSocket()
		}

		now := uint64(time.Now().UnixNano())
		elapsed := now % m.windowSize
		remaining := m.windowSize - elapsed
		time.Sleep(time.Duration(remaining))
	}
}