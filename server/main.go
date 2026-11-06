package main
import (
	"suigetsu/server/listeners"
	"suigetsu/server/queue"
	"suigetsu/server/shell"
)

func main() {
	key := [32]byte{171, 185, 207, 217, 235, 249, 22, 42, 58, 78, 94, 106, 122, 134, 150, 16, 187, 169, 223, 201, 251, 233, 6, 58, 42, 94, 78, 122, 106, 150, 134, 32}
	q, err := queue.NewQueue("suigetsu.db")
	if err != nil {
		panic(err)
	}
	manager := listeners.NewManager(key,300_000_000_000,q)
	go manager.Start()
	sh := shell.NewShell(manager)
	sh.Run()
}