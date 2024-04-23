//go:build !windows && !linux && !darwin && !freebsd && !openbsd && !netbsd && !dragonfly && !solaris && !android

package s2jgo

import "errors"

func Available() bool {
	return false
}

type src2JSON struct{}

func load(_ string) (*src2JSON, error) {
	return nil, errors.New("not implemented")
}

func (s *src2JSON) CallIOCallback(args []string, cap Capability, cb func(data []byte, isStdErr bool)) error {
	return errors.New("not implemented")
}
