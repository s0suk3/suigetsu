package beacon
import (
	"encoding/binary"
	"fmt"
)

const (
	TypeCheckin = 0
	TypeResult = 1
	TypeHeartbeat = 2
)

type Beacon struct {
	ImplantID [16]byte
	Type uint8
	DataLen uint32
	Data []byte
}

func Parse(data []byte) (*Beacon, error) {
	if len(data) < 21 {
		return nil, fmt.Errorf("beacon too short")
	}
	beacon := &Beacon{}
	copy(beacon.ImplantID[:], data[0:16])
	beacon.Type = data[16]
	beacon.DataLen = binary.BigEndian.Uint32(data[17:21])
	if uint64(len(data)) < 21+uint64(beacon.DataLen) {
		return nil, fmt.Errorf("beacon payload truncated: expected %d bytes, got %d", 21+beacon.DataLen, len(data))
	}
	beacon.Data = make([]byte, beacon.DataLen)
	copy(beacon.Data, data[21:21+beacon.DataLen])
	return beacon, nil
}

func (b *Beacon) Serialize() []byte {
	data := make([]byte, 21+len(b.Data))
	copy(data[0:16], b.ImplantID[:])
	data[16] = b.Type
	binary.BigEndian.PutUint32(data[17:21], b.DataLen)
	copy(data[21:], b.Data)
	return data
}