package crypto
import (
    "encoding/binary"
    "time"
    "golang.org/x/crypto/chacha20"
)
//This function is for generating the counter value used for the chacha20
func GetCounter(windowSize uint64) uint64 {
	return uint64(time.Now().UnixNano()) / windowSize
}

//Implementing chacha20
func GetProtocol(key [32]byte, counter uint64) byte {
	counterBytes := make([]byte, 12)
	binary.BigEndian.PutUint64(counterBytes[4:12], counter)
	cipher, _ := chacha20.NewUnauthenticatedCipher(key[:], counterBytes[:])
	result := make([]byte, 1)
	cipher.XORKeyStream(result, result)
	return result[0]%4
}