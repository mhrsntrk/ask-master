VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

.PHONY: build install test lint firmware clean all

build:
	go build -ldflags="-s -w -X main.version=$(VERSION)" -o ask-master .

install:
	go install -ldflags="-s -w -X main.version=$(VERSION)" .

test:
	go test ./... -race -v

lint:
	golangci-lint run ./...

firmware:
	pio run -d firmware/ask_master

clean:
	rm -f ask-master

all: build lint test
