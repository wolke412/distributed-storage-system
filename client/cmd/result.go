package cmd

import "github.com/wolke412/paint"

// CommandStatus defines possible outcomes of a command.
type CommandStatus int

const (
	StatusSuccess CommandStatus = iota
	StatusWarning
	StatusError
)

// CommandResult is returned by every command handler.
type CommandResult struct {
	Status  CommandStatus
	Message string
	Err     error
}

// Helpers to quickly build results.
func Success(msg string) *CommandResult {
	return &CommandResult{Status: StatusSuccess, Message: msg}
}

func Warning(msg string) *CommandResult {
	return &CommandResult{Status: StatusWarning, Message: msg}
}

func Failure(msg string, err error) *CommandResult {
	return &CommandResult{Status: StatusError, Message: msg, Err: err}
}

// PrettyString returns a formatted string for REPL display.
func (r *CommandResult) PrettyString() string {

	r.Message = " "  + r.Message

	switch r.Status {
	case StatusSuccess:
		icon := paint.Both(paint.BRIGHT_WHITE, paint.GREEN, " ✓ ")
		return icon + paint.BrightGreen(r.Message)

	case StatusWarning:
		icon := paint.Both(paint.BRIGHT_WHITE, paint.YELLOW, " ! ")
		return icon + paint.Yellow(r.Message)

	case StatusError:
		if r.Err != nil {
			r.Message = r.Message  + ": " + r.Err.Error()
		}

		icon := paint.Both(paint.BRIGHT_WHITE, paint.BRIGHT_RED, " ✗ ")
		return icon + paint.BrightRed(r.Message)

	default:
		return r.Message
	}
}
