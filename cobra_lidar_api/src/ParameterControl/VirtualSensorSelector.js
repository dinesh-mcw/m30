import React, {Component} from "react";
import ButtonGroup from "react-bootstrap/ButtonGroup";
import Button from "react-bootstrap/Button";
import ToggleButton from "react-bootstrap/ToggleButton";


class VirtualSensorSelector extends Component {
    render() {
        const radios = [
            {value: 1, variant: "outline-success"},
            {value: 2, variant: "outline-warning"},
            {value: 3, variant: "outline-danger"},
        ];

        return (
            <ButtonGroup className={"mb-2 w-75"}>
                <Button variant={"outline-secondary"} disabled>Number Virtual Sensors</Button>
                {radios.map((radio, idx) => {
                    return <ToggleButton
                        name={"vs_radio"}
                        type={"radio"}
                        id={`virtual-sensor-${idx}`}
                        key={idx}
                        value={radio.value}
                        variant={radio.variant}
                        onChange={this.props.onChange}
                        checked={this.props.nVirtualSensor === radio.value}
                    >
                        {`${radio.value}`}
                    </ToggleButton>
                })}
            </ButtonGroup>
        )
    }
}

export default VirtualSensorSelector;
