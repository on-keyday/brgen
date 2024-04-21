package request

import (
	"io"
	"os"
	"os/exec"
)

type ProcessClient struct {
	cmd *exec.Cmd
	SourceManager
}

// NewProcessClient creates a new process client
// stdin and stdout are set to handle the communication
// you should set stderr if you want to see the error
func NewProcessClient(cmd *exec.Cmd) (*ProcessClient, error) {
	rp, ws, err := os.Pipe()
	if err != nil {
		return nil, err
	}
	rs, wp, err := os.Pipe()
	if err != nil {
		return nil, err
	}
	cmd.Stdin = rp
	cmd.Stdout = wp
	err = cmd.Start()
	if err != nil {
		return nil, err
	}
	c := NewRequestManager(rs, ws, func(err error) {
		if err != io.EOF {
			cmd.Process.Kill()
		}
	})
	go func() {
		err := cmd.Wait()
		c.CloseWithError(err)
	}()
	return &ProcessClient{
		cmd:           cmd,
		SourceManager: c,
	}, nil
}
