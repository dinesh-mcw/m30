import React, {Component} from "react";
import Form from "react-bootstrap/Form";
import Col from "react-bootstrap/Col";


class ParameterInput extends Component {
    constructor(props) {
        super(props);

        this.state = {valid: true};
    }

    get value() {
        return this.props.value || 0;
    }

    get min() {
        return this.props.min || this.props.low || 0;
    }

    get max() {
        return this.props.max || this.props.high || 1;
    }

    onChange(event) {
        let valid = this.state.valid;
        let newval = event.target.value;
        if (this.props.type === "number") {
            newval = Number(newval);
            valid = !((newval < this.min) || (newval > this.max));
        }
        if (typeof this.props.onChange === "function") {
            this.props.onChange(valid ? newval : this.props.value)
        }
    }


    render() {
        const min = this.min;
        const max = this.max;
        const title = `Please choose a value between ${min} and ${max}`;

        const {value, param, type, maxLength, step, disabled} = this.props;
        const controlId = `virtual_sensor_Input${param}`;

        return (
            <Form.Group as={Col} controlId={controlId}>
                <Form.Label>{param}</Form.Label>
                <Form.Control
                    type={type || "text"}
                    maxLength={maxLength || 8}
                    title={title}
                    value={value}
                    min={min}
                    max={max}
                    step={step || 1}
                    disabled={disabled || false}
                    onChange={(e) => this.onChange(e)}
                    isInvalid={!this.state.valid}
                    required
                />
                <Form.Control.Feedback type={"invalid"}>{title}</Form.Control.Feedback>
            </Form.Group>
        );
    }
}

export default ParameterInput;
