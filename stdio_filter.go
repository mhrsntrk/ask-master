package main

import (
	"bytes"
	"encoding/json"
	"io"
)

// annotationFilter wraps an io.Writer and strips annotations from MCP tools/list
// JSON-RPC responses. This works around an OpenCode bug where tools with
// annotations silently fail to register (github.com/anomalyco/opencode/issues/12094).
type annotationFilter struct {
	w   io.Writer
	buf []byte
}

func newAnnotationFilter(w io.Writer) *annotationFilter {
	return &annotationFilter{w: w}
}

func (f *annotationFilter) Write(p []byte) (n int, err error) {
	f.buf = append(f.buf, p...)
	for {
		idx := bytes.IndexByte(f.buf, '\n')
		if idx < 0 {
			break
		}
		line := f.buf[:idx]
		f.buf = f.buf[idx+1:]
		if err := f.processLine(line); err != nil {
			return len(p), err
		}
	}
	return len(p), nil
}

func (f *annotationFilter) processLine(line []byte) error {
	modified := stripAnnotations(line)
	if _, err := f.w.Write(modified); err != nil {
		return err
	}
	_, err := f.w.Write([]byte{'\n'})
	return err
}

func stripAnnotations(line []byte) []byte {
	if !bytes.Contains(line, []byte(`"annotations"`)) {
		return line
	}

	var msg map[string]json.RawMessage
	if err := json.Unmarshal(line, &msg); err != nil {
		return line
	}

	result, ok := msg["result"]
	if !ok {
		return line
	}

	var resultMap map[string]json.RawMessage
	if err := json.Unmarshal(result, &resultMap); err != nil {
		return line
	}

	toolsRaw, ok := resultMap["tools"]
	if !ok {
		return line
	}

	var tools []map[string]json.RawMessage
	if err := json.Unmarshal(toolsRaw, &tools); err != nil {
		return line
	}

	modified := false
	for _, tool := range tools {
		if _, has := tool["annotations"]; has {
			delete(tool, "annotations")
			modified = true
		}
	}

	if !modified {
		return line
	}

	newTools, err := json.Marshal(tools)
	if err != nil {
		return line
	}
	resultMap["tools"] = json.RawMessage(newTools)

	newResult, err := json.Marshal(resultMap)
	if err != nil {
		return line
	}
	msg["result"] = json.RawMessage(newResult)

	newLine, err := json.Marshal(msg)
	if err != nil {
		return line
	}
	return newLine
}
