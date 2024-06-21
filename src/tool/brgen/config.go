package main

import "encoding/json"

type ArrayOrString []string

func (a *ArrayOrString) UnmarshalJSON(b []byte) error {
	var arr []string
	if err := json.Unmarshal(b, &arr); err == nil {
		*a = arr
		return nil
	}
	var str string
	if err := json.Unmarshal(b, &str); err != nil {
		return err
	}
	*a = []string{str}
	return nil
}

type Output struct {
	Generator     ArrayOrString `json:"generator"`
	OutputDir     string        `json:"output_dir"`
	Args          []string      `json:"args"`
	IgnoreMissing bool          `json:"ignore_missing"`
}

type Warnings struct {
	DisableUntypedWarning bool `json:"disable_untyped"`
	DisableUnusedWarning  bool `json:"disable_unused"`
}

type Config struct {
	Source2Json    *string   `json:"src2json"`
	LibSource2Json *string   `json:"libs2j"`
	Suffix         *string   `json:"suffix"`
	TargetDirs     []string  `json:"input_dir"`
	Warnings       Warnings  `json:"warnings"`
	Output         []*Output `json:"output"`
	TestInfo       *string   `json:"test_info_output"`
}
