import React, {Component} from "react";
import Form from "react-bootstrap/Form";
import Col from "react-bootstrap/Col";


class ParameterDropdown extends Component {
    render() {
        const param = this.props.param || "Option";
        const value = this.props.value || 0;
        const options = this.props.options || [0];
        const disabled = this.props.disabled || false;
        const onChange = (e) => {
            if (this.props.type === "int") {
                this.props.onChange(parseInt(e.target.value));
            }
            else if (this.props.type === "float") {
                this.props.onChange(parseFloat(e.target.value));
            }
        }

        let title = this.props.title || "";

        if (disabled) {
            let newStr = "This will be available for use on a future date."
            title = [title, newStr].join(" ");
        }

        return (
            <Form.Group as={Col} >
                <Form.Label>{param}</Form.Label>
                <Form.Select
                    value={value}
                    disabled={disabled}
                    title={title}
                    onChange={onChange}
                >
                    {options.map((v) => <option key={`${param}-${v}`}>{v}</option>)}
                </Form.Select>
            </Form.Group>
        );
    }
}

export default ParameterDropdown;
