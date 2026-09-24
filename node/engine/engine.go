package engine

/*
#cgo CXXFLAGS: -std=c++17
#cgo CFLAGS: -I${SRCDIR}/../../engine/include
#cgo LDFLAGS: -L${SRCDIR}/../../engine/build/lib -lengine -lstdc++ -lpthread

#include "engine.h"
#include <stdlib.h>
*/
import "C"
import (
	"errors"
	"time"
	"unsafe"
)

var (
	ErrNotFound      = errors.New("key not found")
	ErrBufferTooSmall = errors.New("buffer too small")
	ErrValueTooLarge  = errors.New("value too large (max 512KB)")
)

// Engine wraps the C++ storage engine via cgo
type Engine struct {
	handle *C.EngineHandle
}

// New creates a new storage engine instance
func New(numThreads int, memLimitBytes uint64) (*Engine, error) {
	handle := C.engine_create(C.int(numThreads), C.size_t(memLimitBytes))
	if handle == nil {
		return nil, errors.New("failed to create engine")
	}
	
	return &Engine{handle: handle}, nil
}

// Close destroys the engine and frees resources
func (e *Engine) Close() {
	if e.handle != nil {
		C.engine_destroy(e.handle)
		e.handle = nil
	}
}

// Set stores a key-value pair with optional TTL
func (e *Engine) Set(key, value string, ttl time.Duration) error {
	if e.handle == nil {
		return errors.New("engine is closed")
	}
	
	keyC := C.CString(key)
	defer C.free(unsafe.Pointer(keyC))
	
	valC := C.CString(value)
	defer C.free(unsafe.Pointer(valC))
	
	ttlNs := C.int64_t(0)
	if ttl > 0 {
		ttlNs = C.int64_t(ttl.Nanoseconds())
	}
	
	result := C.engine_set(e.handle, keyC, C.size_t(len(key)),
		valC, C.size_t(len(value)), ttlNs)
	
	switch result {
	case 0:
		return nil
	case -2:
		return ErrValueTooLarge
	default:
		return errors.New("engine_set failed")
	}
}

// Get retrieves a value by key
func (e *Engine) Get(key string) (string, error) {
	if e.handle == nil {
		return "", errors.New("engine is closed")
	}
	
	keyC := C.CString(key)
	defer C.free(unsafe.Pointer(keyC))
	
	// Allocate buffer (max value size is 512KB)
	bufSize := C.size_t(512 * 1024)
	buf := C.malloc(bufSize)
	defer C.free(buf)
	
	outLen := bufSize
	result := C.engine_get(e.handle, keyC, C.size_t(len(key)),
		(*C.char)(buf), &outLen)
	
	switch result {
	case 0:
		// Success - convert C string to Go string
		return C.GoStringN((*C.char)(buf), C.int(outLen)), nil
	case -1:
		return "", ErrNotFound
	case -2:
		return "", ErrBufferTooSmall
	default:
		return "", errors.New("engine_get failed")
	}
}

// Delete removes a key
func (e *Engine) Delete(key string) error {
	if e.handle == nil {
		return errors.New("engine is closed")
	}
	
	keyC := C.CString(key)
	defer C.free(unsafe.Pointer(keyC))
	
	result := C.engine_delete(e.handle, keyC, C.size_t(len(key)))
	
	switch result {
	case 0:
		return nil
	case -1:
		return ErrNotFound
	default:
		return errors.New("engine_delete failed")
	}
}

// ReplayWAL replays the write-ahead log from the specified directory
func (e *Engine) ReplayWAL(walDir string) error {
	if e.handle == nil {
		return errors.New("engine is closed")
	}
	
	walDirC := C.CString(walDir)
	defer C.free(unsafe.Pointer(walDirC))
	
	result := C.engine_replay_wal(e.handle, walDirC)
	if result < 0 {
		return errors.New("WAL replay failed")
	}
	
	return nil
}

// Snapshot creates a snapshot of the current state
func (e *Engine) Snapshot(path string) error {
	if e.handle == nil {
		return errors.New("engine is closed")
	}
	
	pathC := C.CString(path)
	defer C.free(unsafe.Pointer(pathC))
	
	result := C.engine_snapshot(e.handle, pathC)
	if result != 0 {
		return errors.New("snapshot creation failed")
	}
	
	return nil
}
