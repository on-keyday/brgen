package main

type TEST_CLASS struct {
}

func (t *TEST_CLASS) Decode(data []byte) (int, error) {
	return 0, nil
}

func (t *TEST_CLASS) Encode() ([]byte, error) {
	return nil, nil
}
