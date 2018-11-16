/*
* Copyright Statement:
*
* This software/firmware and related documentation ("MediaTek Software") are
* protected under relevant copyright laws. The information contained herein is
* confidential and proprietary to MediaTek Inc. and/or its licensors. Without
* the prior written permission of MediaTek inc. and/or its licensors, any
* reproduction, modification, use or disclosure of MediaTek Software, and
* information contained herein, in whole or in part, shall be strictly
* prohibited.
*
* MediaTek Inc. (C) 2015. All rights reserved.
*
* BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
* THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
* RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
* ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
* WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
* NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
* RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
* INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
* TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
* RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
* OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
* SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
* RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
* STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
* ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
* RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
* MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
* CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
*/
#include <libxml/parser.h>
#include "module.h"
#include "util.h"

static module* module_create_by_name(char *name)
{
	if (strcmp(name, "fin") == 0) {
		return module_create(fin_module_init);
	} else if (strcmp(name, "fout") == 0) {
		return module_create(fout_module_init);
	} else if (strcmp(name, "v4l2") == 0) {
		return module_create(v4l2_module_init);
	} else if (strcmp(name, "v4l2_mdp") == 0) {
		return module_create(v4l2_mdp_module_init);
	} else if (strcmp(name, "v4l2_camera") == 0) {
		return module_create(v4l2_camera_module_init);
	} else if (strcmp(name, "ppm_logo") == 0) {
		return module_create(ppm_logo_module_init);
	} else if (strcmp(name, "drm") == 0) {
		return module_create(drm_module_init);
	} else if (strcmp(name, "list2va") == 0) {
		return module_create(list2va_module_init);
	} else if (strcmp(name, "va2list") == 0) {
		return module_create(va2list_module_init);
	} else if (strcmp(name, "h264_logo") == 0) {
		return module_create(h264_logo_module_init);
	} else if (strcmp(name, "v4l2_codec") == 0) {
		return module_create(v4l2_codec_module_init);
	} else if (strcmp(name, "fourinone_camera") == 0) {
		return module_create(fourinone_camera_module_init);
	//} else if (strcmp(name, "composite") == 0) {
		//return module_create(composite_module_init);
	} else if (strcmp(name, "v4l2_nr") == 0) {
		return module_create(v4l2_nr_module_init);
	} else if (strcmp(name, "fake") == 0) {
		return module_create(fake_module_init);
	} else if (strcmp(name, "ovl") == 0) {
		return module_create(ovl_module_init);
	}
	LOG_ERR("module %s not support", name);
	return NULL;
}

void* xml_open(char *path)
{
	xmlDocPtr doc;
	xmlNodePtr node;

	LOG_INFO("xml_open start");

	xmlKeepBlanksDefault(0);
	doc = xmlReadFile(path, "UTF-8", XML_PARSE_RECOVER);
	if (doc == NULL) {
		LOG_ERR("XML: %s open fail", path);
		return NULL;
	}
	node = xmlDocGetRootElement(doc);
	if ((node == NULL) || xmlStrcmp(node->name, BAD_CAST "root")) {
		LOG_ERR("XML: root node check fail");
		xmlFreeDoc(doc);
		return NULL;
	}

	LOG_INFO("xml_open done");
	return (void*)doc;
}

void xml_close(void *handle)
{
	if (handle)
		xmlFreeDoc((xmlDocPtr)handle);
}

link_path* xml_create_link(void *handle, char *link_name)
{
	xmlNodePtr root;
	xmlNodePtr linkNode;
	xmlNodePtr moduleNode;
	xmlNodePtr configNode;

	char *dev_name;
	char *dev_path;
	module *mod;
	link_path *ln;

	if (!handle || !link_name)
		return NULL;

	LOG_INFO("xml_create_link(%s) start", link_name);

	/*
	* find out link node
	*/
	root = xmlDocGetRootElement((xmlDocPtr)handle);
	linkNode = root->xmlChildrenNode;
	while (linkNode != NULL) {
		if (!xmlStrcmp(linkNode->name, BAD_CAST link_name))
			break;
		linkNode = linkNode->next;
	}
	if (!linkNode) {
		LOG_ERR("XML: link %s not exist", link_name);
		return NULL;
	}

	/*
	* create all modules
	*/
	ln = NULL;
	moduleNode = linkNode->xmlChildrenNode;
	while (moduleNode) {
		if (xmlStrcmp(moduleNode->name, BAD_CAST "module")) {
				moduleNode = moduleNode->next;
				continue;
		}
		mod = module_create_by_name((char*)xmlGetProp(moduleNode, BAD_CAST "name"));
		if (!mod) {
			LOG_WARN("create module fail");
		}

		/*
		* support dynamic get device path
		* if "name" also configured in <config>, will rewite
		*/
		dev_name = (char*)xmlGetProp(moduleNode, BAD_CAST "dev_name");
		if (dev_name) {
			dev_path = device_get_path(dev_name);
			if (dev_path) {
				module_config(mod, "name", (unsigned long)dev_path);
			} else {
				LOG_ERR("device cannot found: %s", dev_name);
				//return NULL;
			}
		}

		configNode = moduleNode->xmlChildrenNode;
		while (configNode) {
			if (xmlStrcmp(configNode->name, BAD_CAST "config")) {
				configNode = configNode->next;
				continue;
			}
			module_config(mod,
				(char*)xmlGetProp(configNode, BAD_CAST "name"),
				string_to_value((char*)xmlGetProp(configNode, BAD_CAST "type"),
						(char*)xmlGetProp(configNode, BAD_CAST "value"))
			);
			configNode = configNode->next;
		}

		ln = link_modules(ln, mod);
		if (!ln) {
			LOG_ERR("link module fail");
			return NULL;
		}
		moduleNode = moduleNode->next;
	}

	LOG_INFO("xml_create_link done");
	return ln;
}

static link_path * xml_auto_camera_create_link(void *handle)
{
	xmlNodePtr root;
	xmlNodePtr linkNode;
	xmlNodePtr cameraNode;

	root = xmlDocGetRootElement((xmlDocPtr)handle);
	linkNode = root->xmlChildrenNode;
	while (linkNode != NULL) {
		if (!xmlStrcmp(linkNode->name, BAD_CAST "camera_auto_detection"))
			break;
		linkNode = linkNode->next;
	}
	if (!linkNode) {
		LOG_ERR("XML: <camera_auto_detection> not exist");
		return NULL;
	}
	cameraNode = linkNode->xmlChildrenNode;
	while (cameraNode) {
		if (xmlStrcmp(cameraNode->name, BAD_CAST "camera")) {
			cameraNode = cameraNode->next;
			continue;
		}
		if(device_get_path((char*)xmlGetProp(cameraNode, BAD_CAST "dev_name")))
			return xml_create_link(handle, (char*)xmlGetProp(cameraNode, BAD_CAST "link_name"));
		cameraNode = cameraNode->next;
	}
	return NULL;
}

link_path* xml_create_camera(void *handle)
{
	xmlNodePtr root;
	xmlNodePtr linkNode;

	root = xmlDocGetRootElement((xmlDocPtr)handle);
	linkNode = root->xmlChildrenNode;
	while (linkNode) {
		if (!xmlStrcmp(linkNode->name, BAD_CAST "rear_camera"))
			break;
		linkNode = linkNode->next;
	}
	if (!linkNode) {
		LOG_ERR("XML: can't find node <rear_camera>");
		return NULL;
	}
	if (xmlStrcmp(xmlNodeGetContent(linkNode), BAD_CAST "camera_auto_detection")==0) {
		return xml_auto_camera_create_link(handle);
	}
	return xml_create_link(handle, (char*)xmlNodeGetContent(linkNode));
}

link_path* xml_create_static(void *handle)
{
	xmlNodePtr root;
	xmlNodePtr linkNode;

	root = xmlDocGetRootElement((xmlDocPtr)handle);
	linkNode = root->xmlChildrenNode;
	while (linkNode) {
		if (!xmlStrcmp(linkNode->name, BAD_CAST "static_logo"))
			break;
		linkNode = linkNode->next;
	}
	if (!linkNode) {
		LOG_ERR("XML: can't find node <static_logo>");
		return NULL;
	}

	return xml_create_link(handle, (char*)xmlNodeGetContent(linkNode));
}

link_path* xml_create_animated(void *handle)
{
	xmlNodePtr root;
	xmlNodePtr linkNode;

	root = xmlDocGetRootElement((xmlDocPtr)handle);
	linkNode = root->xmlChildrenNode;
	while (linkNode) {
		if (!xmlStrcmp(linkNode->name, BAD_CAST "animated_logo"))
			break;
		linkNode = linkNode->next;
	}
	if (!linkNode) {
		LOG_ERR("XML: can't find node <animated_logo>");
		return NULL;
	}

	return xml_create_link(handle, (char*)xmlNodeGetContent(linkNode));
}

char* xml_get_setting(void *handle, char *name)
{
	xmlNodePtr root;
	xmlNodePtr linkNode;
	xmlNodePtr subNode;

	if (!name)
		return NULL;

	root = xmlDocGetRootElement((xmlDocPtr)handle);
	linkNode = root->xmlChildrenNode;
	while (linkNode) {
		if (!xmlStrcmp(linkNode->name, BAD_CAST "setting"))
			break;
		linkNode = linkNode->next;
	}
	if (!linkNode)
		return NULL;

	subNode = linkNode->xmlChildrenNode;
	while (subNode) {
		if (!xmlStrcmp(subNode->name, BAD_CAST name)) {
				return (char*)xmlGetProp(subNode, BAD_CAST "value");
		}
		subNode = subNode->next;
	}

	return NULL;
}
