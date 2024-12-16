import React, {useMemo, useState, useEffect} from "react";
import "./footer.css";
import globalVariables from "../Common/globalVariables";
import IntervalFetch from "../Common/IntervalFetch";

const {baseUrl} = globalVariables;


const Footer = () => {
    const defaultOsVersion = useMemo(() => <i>Not Connected</i>, []);
    const [osVersion, setOsVersion] = useState(defaultOsVersion);
    const defaultOsBuild = useMemo(() => <i>Not Connected</i>, []);
    const [osBuild, setOsBuild] = useState(defaultOsBuild);
    const defaultFpgaSha = useMemo(() => <i>Not Connected</i>, []);
    const [fpgaSha, setFpgaSha] = useState(defaultFpgaSha);
    const defaultFwSha = useMemo(() => <i>Not Connected</i>, []);
    const [fwSha, setFwSha] = useState(defaultFwSha);
    const defaultOsBuildSha = useMemo(() => <i>Not Connected</i>, []);
    const [osBuildSha, setOsBuildSha] = useState(defaultOsBuildSha);
    const defaultCustLayerSha = useMemo(() => <i>Not Connected</i>, []);
    const [custLayerSha, setCustLayerSha] = useState(defaultCustLayerSha);
    const defaultSensorId = useMemo(() => <i>Not Connected</i>, []);
    const [sensorId, setSensorId] = useState(defaultSensorId);
    const systemVersionUrl = `${baseUrl}/system_version`;
    const sensorIdUrl = `${baseUrl}/sensor_id`;

    useEffect( () => {
        const systemVersionFetcher = new IntervalFetch(systemVersionUrl, {}, async function(data) {
            const osVersion = data.system_version.os_build_version
            const osBuild = data.system_version.os_build_number
            const fpgaSha = data.system_version.fpga_git_sha
            const fwSha = data.system_version.firmware_sha
            const osBuildSha = data.system_version.os_build_sha
            const custLayerSha = data.system_version.cust_layer_sha
            setOsVersion(osVersion);
            setOsBuild(osBuild);
            setFpgaSha(fpgaSha);
            setFwSha(fwSha);
            setOsBuildSha(osBuildSha);
            setCustLayerSha(custLayerSha);
            if (osVersion !== defaultOsVersion) systemVersionFetcher.stop();
        });
        systemVersionFetcher.start();
        return () => systemVersionFetcher.stop();
    }, [defaultOsVersion, defaultOsBuild, defaultFpgaSha, defaultFwSha, defaultOsBuildSha, defaultCustLayerSha, systemVersionUrl]);
    useEffect( () => {
        const sensorIdFetcher = new IntervalFetch(sensorIdUrl, {}, async function(data) {
            const sensorId = data.sensor_id
            setSensorId(sensorId);
            if (sensorId !== defaultSensorId) sensorIdFetcher.stop();
        });
        sensorIdFetcher.start();
        return () => sensorIdFetcher.stop();
    }, [defaultSensorId, sensorIdUrl]);
    const thisYear = new Date().getFullYear().toString();
    const copyText = `Copyright ${thisYear}, Lumotive Inc. All rights reserved.`;
    return (
        <footer id={"footer"}>
            <span>SN: {sensorId}; Release: {osVersion}--{osBuild}; FPGA: {fpgaSha}; Firmware: {fwSha}; OS: {osBuildSha}; cust: {custLayerSha}</span> <br />
            <span>&copy; {copyText}</span>
        </footer>
    );
}

export default Footer;
