package main

import (
	"os/exec"

	"github.com/on-keyday/brgen/ast2go/request"
)

func (g *Generator) initStdinStream() error {
	cmd := exec.CommandContext(g.ctx, g.generatorPath, g.args...)
	cl, err := request.NewProcessClient(cmd)
	if err != nil {
		return err
	}
	g.stdinStream = cl
	return nil
}

func (g *Generator) handleStdinStreamRequest(req *Result) {
	stream := g.stdinStream.CreateStream()
	err := stream.SendRequest(req.Path, req.Data)
	if err != nil {
		req.Err = err
		g.result <- req
		return
	}
	for {
		resp, err := stream.ReceiveResponse()
	}
}
