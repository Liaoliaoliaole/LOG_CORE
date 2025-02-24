/*
Program: Morfeas_opc_ua_config.c, OPC-UA server's configuration.
Copyright (C) 12019-12021  Sam harry Tzavaras

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <open62541/client_config_default.h>
//#include <open62541/network_tcp.h>
#include <open62541/plugin/accesscontrol_default.h>
#include <open62541/plugin/nodestore_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/pki_default.h>
#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/server_config_default.h>

static const size_t usernamePasswordsSize = 1;
static UA_UsernamePasswordLogin usernamePasswords[] = {{UA_STRING_STATIC("Morfeas"), UA_STRING_STATIC("Morfeas_pass")}};

UA_StatusCode Morfeas_OPC_UA_config(UA_ServerConfig *config, const char *app_name, const char *version)
{
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
	char buff[512], hostname[256];

	if(!config)
		return UA_STATUSCODE_BADINVALIDARGUMENT;

	retval = UA_ServerConfig_setBasics(config);
    if(retval != UA_STATUSCODE_GOOD)
    {
        UA_ServerConfig_clear(config);
        return retval;
    }

	config->allowEmptyVariables = UA_RULEHANDLING_ACCEPT; //Allow creation of empty variables without logging them.
	//--Delete default Contents--//
	UA_clear(&(config->buildInfo.productUri), &UA_TYPES[UA_TYPES_STRING]);
	UA_clear(&(config->applicationDescription.applicationUri), &UA_TYPES[UA_TYPES_STRING]);
	UA_clear(&(config->buildInfo.manufacturerName), &UA_TYPES[UA_TYPES_STRING]);
	UA_clear(&(config->buildInfo.productName), &UA_TYPES[UA_TYPES_STRING]);
	UA_clear(&(config->applicationDescription.applicationName.locale), &UA_TYPES[UA_TYPES_STRING]);
	UA_clear(&(config->applicationDescription.applicationName.text), &UA_TYPES[UA_TYPES_STRING]);
	UA_clear(&(config->buildInfo.softwareVersion), &UA_TYPES[UA_TYPES_STRING]);
	//--Load Application's Configuration Contents--//
	gethostname(hostname, sizeof(hostname));
	snprintf(buff, sizeof(buff), "http://%s", hostname);
	config->buildInfo.productUri = UA_STRING_ALLOC(buff);
    config->applicationDescription.applicationUri = UA_STRING_ALLOC("urn:Morfeas.open62541.server.application");
    config->buildInfo.manufacturerName = UA_STRING_ALLOC("Sam-Harry-Tzavaras");
    config->buildInfo.productName = UA_STRING_ALLOC("Morfeas OPC-UA Server (Based on Open62541)");
	config->applicationDescription.applicationName = UA_LOCALIZEDTEXT_ALLOC("en", !app_name?"Morfeas default application":app_name);
	snprintf(buff, sizeof(buff), "Morfeas_opc_ua: v%s, libopen62541: %s", version, UA_OPEN62541_VERSION);
	config->buildInfo.softwareVersion = UA_STRING_ALLOC(buff);
	config->maxSessions = 5;
	config->publishingIntervalLimits.min = 100;
	config->samplingIntervalLimits.min = 100;

	// Allocate the SecurityPolicies
    retval = UA_ServerConfig_addSecurityPolicyNone(config, NULL);// const UA_ByteString *certificate
    if(retval != UA_STATUSCODE_GOOD)
	{
        UA_ServerConfig_clear(config);
        return retval;
    }

    // Initialize the Access Control plugin
    retval = UA_AccessControl_default(config, true, //NULL,
                &config->securityPolicies[config->securityPoliciesSize-1].policyUri,
                usernamePasswordsSize, usernamePasswords);
    if(retval != UA_STATUSCODE_GOOD)
	{
        UA_ServerConfig_clear(config);
        return retval;
    }

    // Allocate the endpoint
    retval = UA_ServerConfig_addEndpoint(config, UA_SECURITY_POLICY_NONE_URI,
                                         UA_MESSAGESECURITYMODE_NONE);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_ServerConfig_clear(config);
        return retval;
    }


	return UA_STATUSCODE_GOOD;
}


