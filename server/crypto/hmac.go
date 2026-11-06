package crypto
import (
	"crypto/hmac"
	"crypto/sha256"
)

//HMAC signature generation
func SignBeacon(key [32]byte, data []byte) []byte {
	h := hmac.New(sha256.New, key[:])
	h.Write(data)
	return h.Sum(nil)
}

//HMAC signature verifying
func Verifybeacon(key [32]byte, data []byte, sig []byte) bool {
	expectedSig := SignBeacon(key, data)
	return hmac.Equal(expectedSig, sig)
}