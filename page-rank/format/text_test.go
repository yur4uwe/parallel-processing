package format

import (
	"page-rank/testdata"
	"testing"
)

func TestParseText(t *testing.T) {
	files, err := testdata.TestFiles.ReadDir(".")
	if err != nil {
		t.Fatal(err)
	}

	for _, file := range files {
		if file.IsDir() {
			continue
		}
		t.Run(file.Name(), func(t *testing.T) {
			f, err := testdata.TestFiles.Open(file.Name())
			if err != nil {
				t.Fatal(err)
			}
			defer f.Close()

			adj, err := ParseText(f, '\t', false)
			if err != nil {
				t.Fatal(err)
			}

			expected := testdata.GetAdjacencyList(t, file.Name())

			testdata.CompareAdjacencyLists(t, expected, adj)
		})
	}
}
