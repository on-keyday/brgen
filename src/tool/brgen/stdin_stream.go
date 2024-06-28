package main

import (
	"errors"
	"io"
	"os/exec"
	"path"
	"path/filepath"

	"github.com/on-keyday/brgen/astlib/ast2go/request"
)

func (g *Generator) initStdinStream() error {
	cmdline := g.cmdline()
	cmd := exec.CommandContext(g.ctx, cmdline[0], cmdline[1:]...)
	cl, err := request.NewProcessClient(cmd)
	if err != nil {
		return err
	}
	g.stdinStream = cl
	return nil
}

func (g *Generator) handleStdinStreamRequest(req *Result) {
	stream := g.stdinStream.CreateStream()
	ext := filepath.Ext(req.Path)
	base := filepath.Base(req.Path)
	baseWithoutExt := base[:len(base)-len(ext)]
	err := stream.SendRequest(baseWithoutExt, req.Data)
	if err != nil {
		req.Err = err
		g.sendResult(req)
		return
	}
	for {
		resp, err := stream.ReceiveResponse()
		if err != nil {
			if err == io.EOF {
				break
			}
			req.Err = err
			g.sendResult(req)
			return
		}
		if resp.Status == request.ResponseStatus_Error {
			req.Err = errors.New(string(resp.ErrorMessage))
			g.sendResult(req)
			continue
		}
		if len(resp.Name) == 0 {
			req.Data = resp.Code
			req.Err = errors.New("empty name")
			g.sendResult(req)
			continue
		}
		if path.Ext(string(resp.Name)) == "" {
			req.Data = resp.Code
			req.Err = errors.New("no extension")
			g.sendResult(req)
			continue
		}
		g.sendGenerated(string(resp.Name), resp.Code)
	}
}
