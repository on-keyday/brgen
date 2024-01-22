//go:build !windows

package s2jgo

import "errors"

type Src2JSON struct{}

func Load(s2j_path string) (*Src2JSON, error) {
	return nil, errors.New("not implemented")
}

func (s *Src2JSON) CallIOCallback(args []string, cap Capability, cb func(data []byte, isStdErr bool)) error {
	return errors.New("not implemented")
}

func (s *Src2JSON) Call(args []string, cap Capability) (*Result, error) {
	return nil, errors.New("not implemented")
}
