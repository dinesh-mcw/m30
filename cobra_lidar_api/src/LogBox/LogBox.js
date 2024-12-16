import React, {Component} from "react";
import Form from "react-bootstrap/Form";

class LogBox extends Component {
    constructor(props) {
        super(props);

        this.ref = React.createRef();
    }

    componentDidUpdate(prevProps, prevState, snapshot) {
        // Scroll to the bottom when adding new messages
        this.ref.current.scrollTop = this.ref.current.scrollHeight;
    }

    getMessagesText(messages, maxMessages=1000) {
        return messages.length > 0 ? messages.slice(-maxMessages).join("\n") : []
    }

    render() {
        const {messages=[], maxMessages=1000, rows=5} = this.props;
        const value = this.getMessagesText(messages, maxMessages);

        return (
                <Form>
                    <Form.Group controlId={"diagnosticLog"}>
                        <Form.Label className={"hidden"}>Diagnostic Log</Form.Label>
                        <Form.Control
                            as="textarea"
                            ref={this.ref}
                            rows={rows}
                            className="w-100 border border-dark rounded"
                            plaintext
                            readOnly
                            value={value}
                        />
                    </Form.Group>
                </Form>
        )
    }
}

export default LogBox;
