import React, {Component} from "react";
import Col from "react-bootstrap/Col";
import Button from "react-bootstrap/Button";

class GridButton extends Component {
    handleClick() {
        if (typeof this.props.onClick === "function") {
            this.props.onClick()
        }
    }

    render() {
        const text = this.props.text || "Preset";
        const variant = this.props.variant || "primary";
        const col = this.props.col || 6;
        const disabled = this.props.disabled || false;

        return (
            <Col col={col} className={"d-flex justify-content-center"}>
                <Button
                    className={"w-100"}
                    variant={variant}
                    onClick={() => this.handleClick()}
                    disabled={disabled}
                >{text}</Button>
            </Col>
        )
    }
}

export default GridButton;
