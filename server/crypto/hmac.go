package crypto
import (
	"crypto/hmac"
	"crypto/sha256"
)

func SignBeacon(key [32]byte, data []byte) []byte {
	h := hmac.New(sha256.New, key[:])
	h.Write(data)
	return h.Sum(nil)
}

func Verifybeacon(key [32]byte, data []byte, sig []byte) bool {
	expectedSig := SignBeacon(key, data)
	return hmac.Equal(expectedSig, sig)
}