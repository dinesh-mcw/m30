import React, {Component} from "react";
import ButtonGroup from "react-bootstrap/ButtonGroup";
import Button from "react-bootstrap/Button";
import ToggleButton from "react-bootstrap/ToggleButton";


class DspModeSelector extends Component {
    render() {
        return (
            <ButtonGroup className={"mb-2 w-75"}>
                <Button variant={"outline-secondary"} disabled>Signal Processing Mode</Button>
                <ToggleButton
                    name={"dsp_radio"}
                    type={"radio"}
                    id={"dsp-mode-0"}
                    key={0}
                    value={0}
                    variant={"outline-primary"}
                    onChange={this.props.onChange}
                    checked={this.props.dsp_mode === 0}
                > Camera Mode
                </ToggleButton>
                <ToggleButton
                    name={"dsp_radio"}
                    type={"radio"}
                    id={"dsp-mode-1"}
                    key={1}
                    value={1}
                    variant={"outline-success"}
                    onChange={this.props.onChange}
                    checked={this.props.dsp_mode === 1}
                > Lidar Mode <span class="badge bg-danger">BETA</span>
                </ToggleButton>
            </ButtonGroup>
        )
    }
}

export default DspModeSelector;
