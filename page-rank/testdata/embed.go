package testdata

import "embed"

//go:embed *.txt
var TestFiles embed.FS
