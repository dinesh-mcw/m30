import React, {Component} from "react";
import ButtonGroup from "react-bootstrap/ButtonGroup";
import Col from "react-bootstrap/Col";
import Button from "react-bootstrap/Button";
import globals from "./Common/globalVariables";

const {baseUrl} = globals;


class ApplySettingsButtons extends Component {
    constructor(props) {
        super(props);

        this.state = {inProgress: false};
    }

    clickApply() {
        // Need to collect angle_range from slider and angle_step
        // from drop down into single setting.
        let arr_length = this.props.settings.angle_step.length
        let new_arr = []
        for (let i = 0; i < arr_length; i++) {
            new_arr.push([this.props.settings.angle_range[i][0],
                          this.props.settings.angle_range[i][1],
                          this.props.settings.angle_step[i]])
        }

        this.props.settings.angle_range=new_arr
        // Create new dict so we can remove the angle_step key
        // for the applySettings call.
        var settings_dict = {...this.props.settings}
        delete settings_dict.angle_step

        this.applySettings(settings_dict);
        }

    applySettings(body, doneCallback, failCallback) {
        this.setState({inProgress: true})

        fetch(`${baseUrl}/scan_parameters`, {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify(body),
        })
            .then(response => {
                this.setState({inProgress: false})
                if (response.ok) {
                    return response.json();
                }
                throw response
            })
            .then(doneCallback)
            .catch(failCallback)
    }

    render() {
        return (
            <ButtonGroup xl={4} lg={4} md={5} sm={8} xs={10} ml={"auto"} as={Col}>
                <Button
                    variant={"success"}
                    onClick={() => this.clickApply()}
                    disabled={this.state.inProgress}
                >
                    Apply Settings
                </Button>
            </ButtonGroup>
        )
    }
}

export default ApplySettingsButtons;
