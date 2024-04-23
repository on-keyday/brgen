//go:build !windows && !linux && !darwin && !freebsd && !openbsd && !netbsd && !dragonfly && !solaris

package s2jgo

import "errors"

func Available() bool {
	return false
}

type Src2JSON struct{}

func Load(s2j_path string) (*Src2JSON, error) {
	return nil, errors.New("not implemented")
}

func (s *Src2JSON) CallIOCallback(args []string, cap Capability, cb func(data []byte, isStdErr bool)) error {
	return errors.New("not implemented")
}
