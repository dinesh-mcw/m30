import React, {Component} from "react";
import Stack from "react-bootstrap/Stack";
import Row from "react-bootstrap/Row";
import GridButton from "../Common/GridButton";

import globals from "../Common/globalVariables";

const {baseUrl} = globals;


const presetParameters = {
    preset1: {
        name: "90 degrees, 10 Hz, high power, 2x2",
        settings: {
            angle_range: [[-45, 45, 1]],
            fps_multiple: [1],
            laser_power_percent: [100],
            inte_time_us: [15],
            max_range_m: [25.2],
            binning: [2],
            snr_threshold: [1.8],
            nn_level: [0],
            user_tag: [0x1],
            interleave: false,
            frame_rate_hz: [960],
            hdr_threshold: 4095,
            hdr_laser_power_percent: [40],
            hdr_inte_time_us: [5],
            dsp_mode: 0,
        }
    },
    preset2: {
        name: "90 degrees, 10 Hz, high power, 4x4",
        settings: {
            angle_range: [[-45, 45, 1]],
            fps_multiple: [1],
            laser_power_percent: [100],
            inte_time_us: [15],
            max_range_m: [25.2],
            binning: [4],
            snr_threshold: [1.8],
            nn_level: [0],
            user_tag: [0x2],
            interleave: false,
            frame_rate_hz: [960],
            hdr_threshold: 4095,
            hdr_laser_power_percent: [40],
            hdr_inte_time_us: [5],
            dsp_mode: 0,
        }
    },
    preset3: {
        name: "90 degrees, 10 Hz, low power, 2x2",
        settings: {
            angle_range: [[-45, 45, 1]],
            fps_multiple: [1],
            laser_power_percent: [40],
            inte_time_us: [15],
            max_range_m: [25.2],
            binning: [2],
            snr_threshold: [1.8],
            nn_level: [0],
            user_tag: [0x3],
            interleave: false,
            frame_rate_hz: [960],
            hdr_threshold: 4095,
            hdr_laser_power_percent: [40],
            hdr_inte_time_us: [5],
            dsp_mode: 0,
        }
    },
    preset4: {
        name: "60 degrees, 15 Hz, high power",
        settings: {
            angle_range: [[-30, 30, 1]],
            fps_multiple: [1],
            laser_power_percent: [100],
            inte_time_us: [15],
            max_range_m: [25.2],
            binning: [2],
            snr_threshold: [1.8],
            nn_level: [0],
            user_tag: [0x4],
            interleave: false,
            frame_rate_hz: [960],
            hdr_threshold: 4095,
            hdr_laser_power_percent: [40],
            hdr_inte_time_us: [5],
            dsp_mode: 0,
        }
    },
    preset5: {
        name: "90 degrees, small angle step horizon",
        settings: {
            angle_range: [[-45, -20, 2], [-20, 20, 0.5], [20, 45, 2]],
            inte_time_us: [15],
            binning: [2],
            snr_threshold: [1.8],
            nn_level: [0],
            fps_multiple: [1],
            user_tag: [1],
            max_range_m: [25.2],
            laser_power_percent: [100],
            frame_rate_hz: [960],
            interleave: false,
            dsp_mode: 0,
            hdr_threshold: 4095,
            hdr_laser_power_percent: [25],
            hdr_inte_time_us: [1]
        }
    },
    preset6: {
        name: "90 degrees, small angle step horizon, high fps ground",
        settings: {
            angle_range: [[-45, -10, 1.5], [-10, 10, 0.4], [10, 45, 3]],
            inte_time_us: [10, 15, 10],
            binning: [2, 1, 4],
            snr_threshold: [1.8],
            nn_level: [0],
            fps_multiple: [4, 2, 1],
            user_tag: [1],
            max_range_m: [25.2],
            laser_power_percent: [100],
            frame_rate_hz: [960],
            interleave: false,
            dsp_mode: 0,
            hdr_threshold: 4095,
            hdr_laser_power_percent: [25],
            hdr_inte_time_us: [1],
        }
    },
    preset7: {
        name: "60 degrees, Low frame rate (450Hz)",
        settings: {
            angle_range: [[-30, 30, 1]],
            fps_multiple: [1],
            laser_power_percent: [100],
            inte_time_us: [15],
            max_range_m: [25.2],
            binning: [2],
            snr_threshold: [1.8],
            nn_level: [0],
            user_tag: [0x9],
            interleave: false,
            frame_rate_hz: [450],
            hdr_threshold: 4095,
            hdr_laser_power_percent: [40],
            hdr_inte_time_us: [5],
            dsp_mode: 0,
        }
    },
    preset8: {
        name: "90 degrees, 10Hz, high power, with HDR",
        settings: {
            angle_range: [[-45, 45, 1]],
            inte_time_us: [15],
            binning: [2],
            snr_threshold: [1.8],
            nn_level: [0],
            fps_multiple: [1],
            user_tag: [1],
            max_range_m: [25.2],
            laser_power_percent: [100],
            frame_rate_hz: [960],
            interleave: false,
            dsp_mode: 0,
            hdr_threshold: 400,
            hdr_laser_power_percent: [25],
            hdr_inte_time_us: [1],
        }
    },
    preset9: {
        name: "40 degrees, high fps ground",
        settings: {
            angle_range: [[-30, -5, 1], [-5, 10, 1]],
            fps_multiple: [5, 1],
            laser_power_percent: [100, 100],
            inte_time_us: [10, 15],
            max_range_m: [25.2, 25.2],
            binning: [2, 2],
            snr_threshold: [1.8, 1.8],
            nn_level: [0, 0],
            user_tag: [0x12, 0x13],
            interleave: false,
            frame_rate_hz: [960, 960],
            hdr_threshold: 4095,
            hdr_laser_power_percent: [40],
            hdr_inte_time_us: [5],
            dsp_mode: 0,
        }
    },

}


class ScanPresets extends Component {
    constructor(props) {
        super(props);

        this.state = {inProgress: false};
    }

    postCustomParameters(params, doneCallback, catchCallback) {
        let body = {...params};

        this.setState({inProgress: true})

        fetch(`${baseUrl}/scan_parameters`, {
            method: "POST",
            headers: {"Content-Type": "application/json"},
            body: JSON.stringify(body)
        })
            .then((response) => {
                this.setState({inProgress: false})
                if (response.ok) {
                    return response.json();
                }
                throw response;
            })
            .then(() => {
                body.nVirtualSensor = body.angle_range.length;
                doneCallback(body)
            })
            .catch(catchCallback)
    }

    render() {
        const {doneCallback, errorCallback} = this.props;
        return (
            <Stack gap={3}>
                <div className={"h4 text-center"}>Preset Scan Parameters</div>
                <Row>
                    <GridButton
                        text={presetParameters.preset1.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset1.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                    <GridButton
                        text={presetParameters.preset2.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset2.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                    <GridButton
                        text={presetParameters.preset3.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset3.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                </Row>
                <Row>
                    <GridButton
                        text={presetParameters.preset4.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset4.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                    <GridButton
                        text={presetParameters.preset5.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset5.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                    <GridButton
                        text={presetParameters.preset6.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset6.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                </Row>
                <Row>
                    <GridButton
                        text={presetParameters.preset7.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset7.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                    <GridButton
                        text={presetParameters.preset8.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset8.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                    <GridButton
                        text={presetParameters.preset9.name}
                        onClick={() => this.postCustomParameters(
                            presetParameters.preset9.settings,
                            doneCallback,
                            errorCallback,
                        )}
                        disabled={this.state.inProgress}
                    />
                </Row>
            </Stack>
        );
    }
}

export default ScanPresets
