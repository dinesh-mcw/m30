import React, {Component} from "react";
import Form from "react-bootstrap/Form";
import Row from "react-bootstrap/Row";
import Col from "react-bootstrap/Col";
import ParameterDropdown from "../Common/ParameterDropdown";
import ParameterInput from "../Common/ParameterInput";
import AngleSlider from "./AngleSlider";


class VirtualSensorParameters extends Component {
    onChange(param, newVal) {
        let ret = {};
        ret[param] = {value: newVal};
        this.props.onChange(ret);
    }

    render() {
        return (
            <Form className={"justify-content-center col-md-9 col-xs-12 mx-auto"}>
                <Row>
                    <ParameterInput
                        param={"Integration Time [\u03bcs]"}
                        type={"number"}
                        step={1}
                        maxlength={2}
                        onChange={(newVal) => this.onChange("inte_time_us", newVal)}
                        {...this.props.inte_time_us}
                    />
                    <ParameterInput
                        param={"Laser Power Percent [%]"}
                        type={"number"}
                        step={1}
                        maxlength={3}
                        onChange={(newVal) => this.onChange("laser_power_percent", newVal)}
                        {...this.props.laser_power_percent}
                    />
                    <ParameterInput
                        param={"Depth Measurement Rate [Hz]"}
                        type={"number"}
                        step={10}
                        maxlength={3}
                        onChange={(newVal) => this.onChange("frame_rate_hz", newVal)}
                        {...this.props.frame_rate_hz}
                    />
                </Row>
                <Row>
                    <ParameterDropdown
                        param={"Pixel Binning Level"}
                        type={"int"}
                        onChange={newVal => this.onChange("binning", newVal)}
                        {...this.props.binning}
                    />
                    <ParameterInput
                        param={"SNR Threshold Filter"}
                        type={"number"}
                        step={0.1}
                        maxlength={4}
                        onChange={(newVal) => this.onChange("snr_threshold", newVal)}
                        {...this.props.snr_threshold}
                    />
                    <ParameterDropdown
                        param={"NN Filter Level"}
                        type={"int"}
                        onChange={newVal => this.onChange("nn_level", newVal)}
                        {...this.props.nn_level}
                    />
                </Row>
                <Row>
                    <ParameterDropdown
                        param={"Max Unambiguous Range"}
                        type={"float"}
                        onChange={newVal => this.onChange("max_range_m", newVal)}
                        {...this.props.max_range_m}
                    />
                    <ParameterInput
                        param={"User Tag"}
                        maxLength={5}
                        onChange={(newVal) => this.onChange("user_tag", newVal)}
                        {...this.props.user_tag}
                    />
                    <ParameterInput
                        param={"FPS Multiple"}
                        type={"number"}
                        maxlength={2}
                        onChange={(newVal) => this.onChange("fps_multiple", newVal)}
                        {...this.props.fps_multiple}
                    />
                </Row>
                <Row className="justify-content-md-center">
                    <Col /> {/* empty spacer */}
                    <ParameterInput
                        param={"Angular Step Size [deg]"}
                        type={"number"}
                        step={0.1}
                        maxlength={3}
                        onChange={(newVal) => this.onChange("angle_step", newVal)}
                        {...this.props.angle_step}
                    />
                    <Col /> {/* empty spacer */}
                </Row>
                <Row>
                    <AngleSlider
                        color={this.props.sliderColor}
                        onSlide={newVal => this.onChange("angle_range", newVal)}
                        {...this.props.angle_range}
                    />
                </Row>
            </Form>
        );
    }
}

export default VirtualSensorParameters;
