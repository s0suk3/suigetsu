package receiver
import (
	"suigetsu/server/crypto"
	"fmt"
	"suigetsu/server/beacon"
)

type Receiver struct {
	key [32]byte
}

func NewReceiver(key [32]byte) *Receiver {
	return &Receiver{
		key: key,
	}
}

func (r *Receiver) Process(data []byte) (*beacon.Beacon, error) {
	if len(data) < 32 {
		return nil, fmt.Errorf("data too short")
	}
	Payload := data[:len(data)-32]
	signature := data[len(data)-32:]
	if !crypto.Verifybeacon(r.key, Payload, signature) {
		return nil, fmt.Errorf("invalid signature")
	}
	return beacon.Parse(Payload)
}