import React, {Component} from "react";
import Tab from "react-bootstrap/Tab";
import Tabs from "react-bootstrap/Tabs"
import VirtualSensorParameters from "./VirtualSensorParameters";


class VirtualSensorTabs extends Component {
    render() {
        return (
            <Tabs
                activeKey={this.props.activeKey}
                defaultActiveKey={"virtual_sensor_1"}
                onSelect={this.props.onSelect}
                id={"virtualSensorSettingsTabs"}
                mb={3}
                className={"d-flex justify-content-center"}
            >
                <Tab eventKey={"virtual_sensor_1"} title={"Virtual Sensor 1"} disabled={this.props.nVirtualSensor <= 0}>
                    <VirtualSensorParameters
                        sliderColor={"green"}
                        userTagDefault={"0xa"}
                        onChange={this.props.onChange}
                        {...this.props.virtual_sensor_1}
                    />
                </Tab>
                <Tab eventKey={"virtual_sensor_2"} title={"Virtual Sensor 2"} disabled={this.props.nVirtualSensor <= 1}>
                    <VirtualSensorParameters
                        sliderColor={"yellow"}
                        userTagDefault={"0xb"}
                        onChange={this.props.onChange}
                        {...this.props.virtual_sensor_2}
                    />
                </Tab>
                <Tab eventKey={"virtual_sensor_3"} title={"Virtual Sensor 3"} disabled={this.props.nVirtualSensor <= 2}>
                    <VirtualSensorParameters
                        sliderColor={"red"}
                        userTagDefault={"0xc"}
                        onChange={this.props.onChange}
                        {...this.props.virtual_sensor_3}
                    />
                </Tab>
            </Tabs>
        );
    }
}

export default VirtualSensorTabs
