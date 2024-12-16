import React, {Component} from "react";
import Stack from "react-bootstrap/Stack";
import Row from "react-bootstrap/Row";
import GridButton from "../Common/GridButton";
import StatusButton from "./StatusButton";
import IntervalFetch from "../Common/IntervalFetch.js";
import globals from "../Common/globalVariables"

const {baseUrl} = globals;

// System state links
const shutdownUrl = `${baseUrl}/shutdown`;


class ScanControlStack extends Component {
    constructor(props) {
        super(props);

        const _this = this;
        this.state = {
            state: "INITIALIZED",
            inProgress: false,
        }
        this.fetcher = new IntervalFetch(`${baseUrl}/state`, { method: "GET" }, async (data) => _this.setState(data));
    }

    componentDidMount() {
        this.fetcher.start();
    }

    componentWillUnmount() {
        this.fetcher.stop();
    }

    post(url, doneCallback, failCallback) {
        fetch(url, {method: "POST"})
            .then(response => {
                if (response.ok) {
                    return response.json()
                }
                throw response;
            })
            .then(doneCallback)
            .catch(failCallback)
    }

    startScan(all=false, doneCallback, failCallback) {
        let init = {method: "POST"};
        this.setState({inProgress: true});

        fetch(`${baseUrl}/start_scan`, init)
            .then((response) => {
                this.setState({inProgress: false})
                if (response.ok) {
                    return response.json();
                }
                throw response
            })
            .then(doneCallback)
            .catch(failCallback)
    }

    stopScan(all=false, doneCallback, failCallback) {
       let init = {method: "POST"};
       this.setState({inProgress: true});

       fetch(`${baseUrl}/stop_scan`, init)
            .then((response) => {
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
            <Stack gap={3} className={"mx-auto"}>
                <div className={"h4 text-center"}>System Status and Control</div>
                <StatusButton state={this.state.state}/>
                <Row>
                    <GridButton
                        variant={"success"}
                        text={"Start Scan"}
                        disabled={this.state.inProgress || this.state.state === "SCANNING"}
                        onClick={() => this.startScan()}
                    />
                    <GridButton
                        variant={"warning"}
                        text={"Stop Scan"}
                        disabled={this.state.inProgress || this.state.state === "ENERGIZED"}
                        onClick={() => this.stopScan()}
                    />
                </Row>
                {
                <Row>
                    <GridButton
                    variant={"danger"}
                    text={"Shutdown System"}
                    disabled={this.state.inProgress}
                    onClick={() => this.post(shutdownUrl)}
                    />
                </Row>
                }
            </Stack>
        );
    }
}

export default ScanControlStack
