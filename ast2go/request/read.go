package request

type SourceStream struct {
}

type SourceStreams struct {
	streams map[uint64]*SourceStream
	nextID  uint64
}
