/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package cmd

import (
	"strconv"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/proto/cloudpb"
	cliUtils "px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils"
)

func init() {
	ConfigCmd.PersistentFlags().StringP("cluster_id", "c", "", "The ID of the cluster to get/update the config for")

	UpdateConfigCmd.Flags().StringP("passthrough", "t", "", "Whether pasthrough should be enabled")
	viper.BindPFlag("passthrough", UpdateConfigCmd.Flags().Lookup("passthrough"))
	UpdateConfigCmd.Flags().StringP("auto_update", "u", "", "Whether auto-updates should be enabled")
	viper.BindPFlag("auto_update", UpdateConfigCmd.Flags().Lookup("auto_update"))

	ConfigCmd.AddCommand(GetConfigCmd)
	ConfigCmd.AddCommand(UpdateConfigCmd)
}

// ConfigCmd is the "config" command for getting/updating the cluster config.
var ConfigCmd = &cobra.Command{
	Use:   "config",
	Short: "Get/update the current cluster config",
}

// GetConfigCmd is the "config get" command.
var GetConfigCmd = &cobra.Command{
	Use:   "get",
	Short: "Get the config for a cluster",
	Run: func(cmd *cobra.Command, args []string) {
		// Check cluster ID.
		clusterID, _ := cmd.Flags().GetString("cluster_id")
		if clusterID == "" {
			cliUtils.Error("Need to specify cluster ID in flags: --cluster_id=<cluster-id>")
			return
		}
		clusterUUID, err := uuid.FromString(clusterID)
		if err != nil {
			cliUtils.Errorf("Invalid cluster ID: %s\n", err.Error())
			return
		}

		cloudAddr := viper.GetString("cloud_addr")
		l, err := vizier.NewLister(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}

		vzInfo, err := l.GetVizierInfo(clusterUUID)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to get Vizier info")
		}

		if len(vzInfo) == 0 {
			cliUtils.Fatalf("Invalid cluster ID: %s", clusterID)
		}

		cliUtils.Infof("%s: %t", "PassthroughEnabled", vzInfo[0].Config.PassthroughEnabled)
		cliUtils.Infof("%s: %t", "AutoUpdateEnabled", vzInfo[0].Config.AutoUpdateEnabled)
	},
}

// UpdateConfigCmd is the "config update" command.
var UpdateConfigCmd = &cobra.Command{
	Use:   "update",
	Short: "Update the config for a cluster",
	Run: func(cmd *cobra.Command, args []string) {
		// Check cluster ID.
		clusterID, _ := cmd.Flags().GetString("cluster_id")
		if clusterID == "" {
			cliUtils.Error("Need to specify cluster ID in flags: --cluster_id=<cluster-id>")
			return
		}
		clusterUUID, err := uuid.FromString(clusterID)
		if err != nil {
			cliUtils.Errorf("Invalid cluster ID: %s\n", err.Error())
			return
		}
		clusterIDPb := utils.ProtoFromUUID(clusterUUID)

		cloudAddr := viper.GetString("cloud_addr")
		l, err := vizier.NewLister(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}

		ptEnabled, _ := cmd.Flags().GetString("passthrough")
		auEnabled, _ := cmd.Flags().GetString("auto_update")

		if ptEnabled == "" && auEnabled == "" {
			return // No config settings specified.
		}

		update := &cloudpb.VizierConfigUpdate{}

		if ptEnabled != "" {
			if pt, err := strconv.ParseBool(ptEnabled); err == nil {
				update.PassthroughEnabled = &types.BoolValue{Value: pt}
			} else {
				cliUtils.Errorf("Invalid value provided for passthrough: %s", err.Error())
			}
		}
		if auEnabled != "" {
			if au, err := strconv.ParseBool(auEnabled); err == nil {
				update.AutoUpdateEnabled = &types.BoolValue{Value: au}
			} else {
				cliUtils.Errorf("Invalid value provided for auto_update: %s", err.Error())
			}
		}

		req := &cloudpb.UpdateClusterVizierConfigRequest{
			ID:           clusterIDPb,
			ConfigUpdate: update,
		}

		err = l.UpdateVizierConfig(req)
		if err != nil {
			cliUtils.Errorf("Error updating config: %s", err.Error())
		} else {
			cliUtils.Info("Successfully updated config")
		}
	},
}
