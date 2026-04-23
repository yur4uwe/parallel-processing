package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"math"
	"os"
	"os/exec"
	"strconv"
	"strings"
)

func main() {
	input := flag.String("input", "/input", "Initial input path in HDFS")
	outputBase := flag.String("output", "/output", "Base output path in HDFS")
	maxIter := flag.Int("iter", 10, "Maximum iterations")
	tol := flag.Float64("tol", 0.001, "Convergence tolerance")
	damping := flag.Float64("damping", 0.85, "Damping factor")
	totalNodes := flag.Int("total", 1, "Total number of nodes")
	hadoopJar := flag.String("jar", "/opt/hadoop/share/hadoop/tools/lib/hadoop-streaming-3.3.6.jar", "Path to hadoop-streaming jar")
	mapperBin := flag.String("mapper", "/opt/binaries/mapper", "Path to mapper binary in container")
	reducerBin := flag.String("reducer", "/opt/binaries/reducer", "Path to reducer binary in container")
	flag.Parse()

	currentInput := *input
	var danglingMass float64

	for i := range *maxIter {
		currentOutput := fmt.Sprintf("%s_iter%d", *outputBase, i)
		fmt.Printf("--- Iteration %d (Dangling Mass: %.6f) ---\n", i, danglingMass)

		// 1. Run Hadoop Job
		err := runHadoopJob(*hadoopJar, mapperBin, reducerBin, currentInput, currentOutput, *damping, *totalNodes, danglingMass)
		if err != nil {
			log.Fatalf("Job failed at iteration %d: %v", i, err)
		}

		// 2. Check Convergence and get dangling mass for next round
		diff, nextDangling, err := calculateDiffAndDangling(currentInput, currentOutput)
		if err != nil {
			log.Printf("Warning: Could not calculate diff: %v", err)
		} else {
			fmt.Printf("Average change: %.6f\n", diff)
			if diff < *tol {
				fmt.Printf("Converged after %d iterations.\n", i)
				break
			}
		}

		// 3. Prepare for next iteration
		currentInput = currentOutput
		danglingMass = nextDangling
	}
}

func runHadoopJob(jar string, mapper, reducer *string, input, output string, damping float64, total int, dMass float64) error {
	// Build the command
	args := []string{
		"jar", jar,
		"-mapper", *mapper,
		"-reducer", fmt.Sprintf("%s -damping %f -total %d -danglingMass %f", *reducer, damping, total, dMass),
		"-input", input,
		"-output", output,
	}

	cmd := exec.Command("hadoop", args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	fmt.Printf("Executing: hadoop %s\n", strings.Join(args, " "))
	return cmd.Run()
}

func calculateDiffAndDangling(oldPath, newPath string) (float64, float64, error) {
	oldRanks, _, err := getRanks(oldPath)
	if err != nil {
		return 0, 0, err
	}
	newRanks, nextDangling, err := getRanks(newPath)
	if err != nil {
		return 0, 0, err
	}

	totalDiff := 0.0
	count := 0
	for id, newRank := range newRanks {
		if id == "__DANGLING__" {
			continue
		}
		if oldRank, ok := oldRanks[id]; ok {
			totalDiff += math.Abs(newRank - oldRank)
			count++
		}
	}

	if count == 0 {
		return 1.0, nextDangling, nil
	}
	return totalDiff / float64(count), nextDangling, nil
}

func getRanks(path string) (map[string]float64, float64, error) {
	cmd := exec.Command("hadoop", "fs", "-cat", path+"/part-*")
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return nil, 0, err
	}

	if err := cmd.Start(); err != nil {
		return nil, 0, err
	}

	ranks := make(map[string]float64)
	dangling := 0.0
	scanner := bufio.NewScanner(stdout)
	for scanner.Scan() {
		parts := strings.Split(scanner.Text(), "\t")
		if len(parts) >= 2 {
			id := parts[0]
			val, _ := strconv.ParseFloat(parts[1], 64)
			if id == "__DANGLING__" {
				dangling = val
			} else {
				ranks[id] = val
			}
		}
	}

	cmd.Wait()
	return ranks, dangling, nil
}
