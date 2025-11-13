package cmd

import "net"

type ConnectionState int

const (
	StateDisconnected ConnectionState = iota
	StateConnecting
	StateConnected
)

type ClientState struct {
	State       ConnectionState
	Conn        net.Conn
	ConnectedTo string
	Console     *Console
}

func NewClientState() *ClientState {
	return &ClientState{
		State:   StateDisconnected,
		Console: NewConsole(),
	}
}

func (s *ClientState) SetState(newState ConnectionState) {
	s.State = newState
}
