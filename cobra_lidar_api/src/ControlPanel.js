import React, {Component} from "react";
import Container from "react-bootstrap/Container";
import Row from "react-bootstrap/Row";
import Col from "react-bootstrap/Col";
import ApplySettingsButtons from "./ApplySettingsButtons";
import DiagnosticLog from "./DiagnosticLog";
import ScanCanvas from "./Canvas/ScanCanvas";
import ScanPresets from "./ParameterControl/ScanPresets";
import VirtualSensorSelector from "./ParameterControl/VirtualSensorSelector";
import VirtualSensorTabs from "./ParameterControl/VirtualSensorTabs";
import ScanControlStack from "./SystemStatus/ScanControlStack";
import DspModeSelector from "./ParameterControl/DspModeSelector";

import globals from "./Common/globalVariables";

const {baseUrl} = globals;


function clamp(num, min, max) {
    return Math.min(Math.max(num, min), max)
}


class ControlPanel extends Component {
    constructor(props) {
        super(props);
        this.divRef = React.createRef();
        this.state = {
            nVirtualSensor: 1,
            activeKey: "virtual_sensor_1",
            dsp_mode: {
                options: [0, 1],
                value: 0,
            },
            interleave: {
                options: [false, true],
                value: true
            },
            virtual_sensor_1: {
                angle_range: {
                    low: -45,
                    high: 45,
                    value: [-45, 45],
                    start: [-45, 45],
                },
                angle_step: {
                    low: 0.2,
                    high: 10,
                    value: 1,
                },
                user_tag: {
                    low: 0,
                    high: 4095,
                    value: this.props.userTagDefault || 10,
                },
                snr_threshold: {
                    low: 0,
                    high: 500,
                    value: 1.8,
                },
                nn_level: {
                    options: [0, 1, 2, 3, 4, 5],
                    value: 0,
                },
                laser_power_percent: {
                    low: 40,
                    high: 100,
                    value: 100,
                },
                inte_time_us: {
                    low: 5,
                    high: 20,
                    value: 15,
                },
                binning: {
                    options: [1,2,4],
                    value: 2,
                },
                max_range_m: {
                    options: [25.2, 32.4],
                    value: 25.2,
                },
                fps_multiple: {
                    low: 1,
                    high: 31,
                    value: 1,
                },
                frame_rate_hz: {
                    low: 300,
                    high: 960,
                    value: 960,
                },
            },
            virtual_sensor_2: {
                angle_range: {
                    low: -45,
                    high: 45,
                    value: [-10, 10],
                    start: [-10, 10],
                },
                angle_step: {
                    low: 0.2,
                    high: 10,
                    value: 1,
                },
                user_tag: {
                    low: 0,
                    high: 4095,
                    value: this.props.userTagDefault || 20,
                },
                snr_threshold: {
                    low: 0,
                    high: 511,
                    value: 1.25,
                },
                nn_level: {
                    options: [0, 1, 2, 3, 4, 5],
                    value: 0,
                },
                laser_power_percent: {
                    low: 40,
                    high: 100,
                    value: 100,
                },
                inte_time_us: {
                    low: 5,
                    high: 20,
                    value: 15,
                },
                binning: {
                    options: [1,2,4],
                    value: 2,
                },
                max_range_m: {
                    options: [25.2, 32.4],
                    value: 25.2,
                },
                fps_multiple: {
                    low: 1,
                    high: 31,
                    value: 1,
                },
                frame_rate_hz: {
                    low: 300,
                    high: 960,
                    value: 960,
                },
            },
            virtual_sensor_3: {
                angle_range: {
                    low: -45,
                    high: 45,
                    value: [-5, 5],
                    start: [-5, 5],
                },
                angle_step: {
                    low: 0.2,
                    high: 10,
                    value: 1,
                },
                user_tag: {
                    low: 0,
                    high: 4095,
                    value: this.props.userTagDefault || 30,
                },
                snr_threshold: {
                    low: 0,
                    high: 511,
                    value: 1.25,
                },
                nn_level: {
                    options: [0, 1, 2, 3, 4, 5],
                    value: 0,
                },
                laser_power_percent: {
                    low: 40,
                    high: 100,
                    value: 100,
                },
                inte_time_us: {
                    low: 5,
                    high: 20,
                    value: 15,
                },
                binning: {
                    options: [1,2,4],
                    value: 2,
                },
                max_range_m: {
                    options: [25.2, 32.4],
                    value: 25.2,
                },
                fps_multiple: {
                    low: 1,
                    high: 31,
                    value: 1,
                },
                frame_rate_hz: {
                    low: 300,
                    high: 960,
                    value: 960,
                },
            },
        }

    }

    get activeKey() {
        return this.checkActiveKey(this.state.activeKey)
    }

    updateValues(key, newValues) {
        let newState = {};
        newState[key] = {};
        for (const param in this.state[key]) {

            let inner = {
                ...this.state[key][param],
                ...newValues[param] || {},
            }
            if (("options" in inner) && (!inner.options.includes(inner.value))) {
                inner.value = inner.options[0]
            }

            newState[key][param] = inner
        }
        return newState;
    }

    checkActiveKey(key) {
        const virtualSensorNum = clamp(parseInt(key.slice(-1)), 1, this.state.nVirtualSensor)
        return `virtual_sensor_${virtualSensorNum}`
    }

    setActiveKey(key) {
        const newKey = this.checkActiveKey(key);
        let newState = this.updateValues(
            newKey,
            {angle_range: {start: this.state[newKey].angle_range.value}},
        )
        newState.activeKey = newKey;
        this.setState(newState)
    }

    getAngleRanges() {
        let values = [];
        switch (this.state.nVirtualSensor) {
            case 3:
                values.unshift(this.state.virtual_sensor_3.angle_range.value);
            // eslint-disable-next-line no-fallthrough
            case 2:
                values.unshift(this.state.virtual_sensor_2.angle_range.value);
            // eslint-disable-next-line no-fallthrough
            case 1:
                values.unshift(this.state.virtual_sensor_1.angle_range.value);
                break;
            default:
                values.push([-30, 30])
        }
        return values;
    }

    getSettings() {
        let states = [];
        let values = {};

        // Add in the interleave parameter since it is not part
        // of the UI by default
        values["interleave"] = this.state.interleave.value
        values["dsp_mode"] = this.state.dsp_mode.value
        // Add in the hdr parameters since they are not part
        // of the UI by default
        values["hdr_threshold"] = 4095
        values["hdr_laser_power_percent"] = 5
        values["hdr_inte_time_us"] = 1

        switch (this.state.nVirtualSensor) {
            case 3:
                states.unshift(this.state.virtual_sensor_3)
            // eslint-disabled-next-line no-fallthrough -- We want previous state
            case 2:
                states.unshift(this.state.virtual_sensor_2)
            // eslint-disabled-next-line no-fallthrough -- We want previous state
            case 1:
                states.unshift(this.state.virtual_sensor_1)
                break;
            default:
                // Do nothing
                return values;
        }

        for (const state of states) {
            for (const param in state) {
                if (typeof values[param] === "object") {
                    values[param].push(state[param].value)
                } else {
                    values[param] = [state[param].value]
                }
            }
        }

        return values;
    }

    getParameterOptions() {
        fetch(`${baseUrl}/scan_parameters/opts`, {
            method: "GET",
            mode: "cors",
            headers: {
                "Access-Control-Allow-Origin": "*"
            }
        })
            .then((res) => {
                if (res.ok) {
                    return res.json();
                }
                throw res;
            })
            .then(data => this.setState(
                {
                    ...this.updateValues("virtual_sensor_1", data),
                    ...this.updateValues("virtual_sensor_2", data),
                    ...this.updateValues("virtual_sensor_3", data),
                }
            ))
            .catch(error => {console.error(error)})
    }

    onChange(newVal) {
        const newState = this.updateValues(this.activeKey, newVal);
        this.setState(newState)
    }

    onApplyPreset(newSettings) {
        let data = {
            virtual_sensor_1: {},
            virtual_sensor_2: {},
            virtual_sensor_3: {},
        };


        const {nVirtualSensor, activeKey} = newSettings;

        for (const param in newSettings) {

            if ((param === "nVirtualSensor") || (param === "activeKey")) {
                continue;
            }

            // Default to HDR off in the UI with Apply Settings button that doesn't have
            // these parameters exposed.
            if (param === 'hdr_threshold') {
                data['virtual_sensor_1']['hdr_threshold'] = {value: newSettings[param].value};
                data['virtual_sensor_2']['hdr_threshold'] = {value: newSettings[param].value};
                data['virtual_sensor_3']['hdr_threshold'] = {value: newSettings[param].value};
                continue;
            }

            if (param === 'hdr_laser_power_percent') {
                data['virtual_sensor_1']['hdr_laser_power_percent'] = {value: newSettings[param].value};
                data['virtual_sensor_2']['hdr_laser_power_percent'] = {value: newSettings[param].value};
                data['virtual_sensor_2']['hdr_laser_power_percent'] = {value: newSettings[param].value};
                continue;
            }

            if (param === 'hdr_inte_time_us') {
                data['virtual_sensor_1']['hdr_inte_time_us'] = {value: newSettings[param].value};
                data['virtual_sensor_2']['hdr_inte_time_us'] = {value: newSettings[param].value};
                data['virtual_sensor_3']['hdr_inte_time_us'] = {value: newSettings[param].value};
                continue;
            }

            if (param === 'interleave') {
                data['virtual_sensor_1']['interleave'] = {value: newSettings[param].value};
                data['virtual_sensor_2']['interleave'] = {value: newSettings[param].value};
                data['virtual_sensor_3']['interleave'] = {value: newSettings[param].value};
                continue;
            }

            if (param === "dsp_mode") {
                data['dsp_mode'] = {value: newSettings[param]};
                continue;
            }

            newSettings[param].forEach((val, idx) => {
                const virtualSensorSel = `virtual_sensor_${idx + 1}`;
                data[virtualSensorSel][param] = {value: val};
                if (param === "angle_range") {
                    data[virtualSensorSel][param].start = val;
                }
            })
        }

        let newState = {
            nVirtualSensor: nVirtualSensor || this.state.nVirtualSensor,
            dsp_mode: data.dsp_mode,
            activeKey: activeKey || "virtual_sensor_1",
            ...this.updateValues("virtual_sensor_1", data.virtual_sensor_1),
            ...this.updateValues("virtual_sensor_2", data.virtual_sensor_2),
            ...this.updateValues("virtual_sensor_3", data.virtual_sensor_3),
        };
        // Get the right angle_step value from the angle_range triplet
        newState.virtual_sensor_1.angle_step.value = newState.virtual_sensor_1.angle_range.value[2]
        newState.virtual_sensor_2.angle_step.value = newState.virtual_sensor_2.angle_range.value[2]
        newState.virtual_sensor_3.angle_step.value = newState.virtual_sensor_3.angle_range.value[2]
        this.setState(newState);

    }

    componentDidMount() {
        this.getParameterOptions();
    }

    render() {
        return (
            <Container>
                <Row className={"py-2"}>
                    {/*Scan Control Column*/}
                    <Col md={6} className={"py-2"}>
                        <ScanControlStack/>
                    </Col>

                    {/*Diagnostic Log*/}
                    <Col md={6} className={"py-2"}>
                        <DiagnosticLog/>
                    </Col>
                </Row>

                <Row className={"py-2"}>
                    <Col md={6}>
                        <ScanPresets
                            doneCallback={this.onApplyPreset.bind(this)}
                        />
                    </Col>

                    <Col md={6} className={"d-flex justify-content-center"} ref={this.divRef}>
                        <ScanCanvas
                            ref={this.divRef}
                            angleRanges={this.getAngleRanges()}
                            activekey={this.activeKey}
                        />
                    </Col>
                </Row>

                <Row className={"justify-content-center py-2"}>
                    <div style={{height: 20}}></div>

                    <div className={"h4 text-center"}>Custom Scan Parameters</div>

                    <div style={{height: 20}}></div>

                    <DspModeSelector
                        dsp_mode={this.state.dsp_mode.value}
                        onChange={e => this.setState(
                            {dsp_mode: {
                                value: parseInt(e.target.value)}
                            },
                        )}
                    />
                    <div style={{height: 20}}></div>

                    <VirtualSensorSelector
                        nVirtualSensor={this.state.nVirtualSensor}
                        onChange={e => this.setState(
                            {nVirtualSensor: parseInt(e.target.value)},
                        )}
                    />

                    <VirtualSensorTabs
                        nVirtualSensor={this.state.nVirtualSensor}
                        virtual_sensor_1={this.state.virtual_sensor_1}
                        virtual_sensor_2={this.state.virtual_sensor_2}
                        virtual_sensor_3={this.state.virtual_sensor_3}
                        activeKey={this.activeKey}
                        onSelect={k => this.setActiveKey(k)}
                        onChange={(newVal) => this.onChange(newVal)}
                    />

                    <ApplySettingsButtons
                        settings={this.getSettings()}
                        activeKey={this.activeKey}
                    />
                </Row>

            </Container>
        );
    }
}

export default ControlPanel;
