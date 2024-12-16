import React, {Component} from "react";
import Button from "react-bootstrap/Button";


class StatusButton extends Component {
    constructor(props) {
        super(props);

        this.blinkId = null;
        this.state = {active: false};
    }

    blink() {
        if (this.props.state === "SCANNING") {
            this.setState((oldState) => ({
                active: !oldState.active
            }))
        } else if (this.state.active) {
            this.setState({active: false})
        }
    }

    componentDidMount() {
        this.blinkId = setInterval(
            () => this.blink(),
            500,
        )
    }

    componentWillUnmount() {
        clearInterval(this.blinkId);
        this.blinkId = null;
    }

    render() {
        let text = "";
        let classes = ["sys-status"]
        if (this.state.active) {
            classes.push("active");
        }
        switch (this.props.state) {
            default:  // Default to initialized
            case "INITIALIZED":
                text = "Initializing...";
                classes.push("configure")
                break;
            case "CONNECTED":
                text = "Connected..."
                classes.push("configure")
                break;
            case "READY":
                text = "Ready";
                classes.push("ready")
                break;
            case "ENERGIZED":
                text = "Energized";
                classes.push("energized")
                break;
            case "SCANNING":
                text = "System is Scanning";
                classes.push("scanning")
                break;
        }

        return (
            <Button variant={"secondary"} className={classes.join(" ")}>{text}</Button>
        );
    }
}

export default StatusButton;
