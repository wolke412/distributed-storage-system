package cmd

import (
	"bufio"
	"fmt"
	"os"
	"strings"
)

type Console struct {
	logs   []string
	width  int
	header string
}

// NewConsole creates a new pretty terminal interface
func NewConsole() *Console {
	return &Console{
		logs:   make([]string, 0),
		width:  getTermWidth(),
		header: "DISTRIBUTED CLIENT v0.1",
	}
}

func getTermWidth() int {
	// fallback if can't detect
	return 80
}

func (c *Console) AddLog(line string) {

	c.logs = append(c.logs, line)

	logSize := 15

	if len(c.logs) > logSize {
		c.logs = c.logs[1:] // keep last 10
	}
}

func (c *Console) ModifyLog(index int, line string) {
	if index < 0 {
		c.logs[len((c.logs))+index] = line
	} else {
		c.logs[index] = line
	}

}

func (c *Console) Draw() {
	clearScreen()
	line := strings.Repeat("-", c.width)
	fmt.Println(line)
	fmt.Println(centerText(c.header, c.width))
	fmt.Println(line)

	for _, log := range c.logs {
		fmt.Println(log)
	}

	fmt.Println(line)
	fmt.Print("[COMMAND]: ")
}

func centerText(text string, width int) string {
	padding := (width - len(text)) / 2
	if padding < 0 {
		padding = 0
	}
	return fmt.Sprintf("%s%s", strings.Repeat(" ", padding), text)
}

func clearScreen() {
	fmt.Print("\033[2J\033[H") // ANSI clear + home
}

func (c *Console) Prompt() string {
	c.Draw()
	reader := bufio.NewReader(os.Stdin)
	line, _ := reader.ReadString('\n')
	return strings.TrimSpace(line)
}
