package main

type Output struct {
	Generator     string   `json:"generator"`
	OutputDir     string   `json:"output_dir"`
	Args          []string `json:"args"`
	IgnoreMissing bool     `json:"ignore_missing"`
}

type Warnings struct {
	DisableUntypedWarning bool `json:"disable_untyped"`
	DisableUnusedWarning  bool `json:"disable_unused"`
}

type Config struct {
	Source2Json *string   `json:"src2json"`
	Suffix      *string   `json:"suffix"`
	TargetDirs  []string  `json:"input_dir"`
	Warnings    Warnings  `json:"warnings"`
	Output      []*Output `json:"output"`
	TestInfo    *string   `json:"test_info_output"`
}
