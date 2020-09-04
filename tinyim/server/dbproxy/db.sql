-- users
-- groups
-- group_

DROP DATABASE IF EXISTS tinyim;
CREATE DATABASE tinyim DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;
USE tinyim;

-- DROP TABLE IF EXISTS `groups`;
-- DROP TABLE IF EXISTS `users`;

CREATE TABLE `groups` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,

  `group_id` bigint(20) NOT NULL,
  `name` varchar(256) COLLATE utf8mb4_bin NOT NULL DEFAULT '',
  -- `avatar` varchar(256) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '群头像',
  `creator` bigint(20) unsigned NOT NULL,
  -- `type` tinyint(3) unsigned NOT NULL DEFAULT '1',
  -- `userCnt` int(11) unsigned NOT NULL DEFAULT '0',
  `status` tinyint(3) unsigned NOT NULL DEFAULT '1',
  -- `version` int(11) unsigned NOT NULL DEFAULT '1',
  -- `lastChated` int(11) unsigned NOT NULL DEFAULT '0',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
  PRIMARY KEY (`id`),
  KEY `idx_name` (`name`(191)),
  KEY `idx_creator` (`creator`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `group_members` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,

  `group_id` bigint(20) NOT NULL,
  `group_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,

  `user_id` bigint(20) NOT NULL,
  `user_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,

  -- `avatar` varchar(256) COLLATE utf8mb4_bin NOT NULL DEFAULT '' COMMENT '群头像',
  -- `type` tinyint(3) unsigned NOT NULL DEFAULT '1',
  -- `userCnt` int(11) unsigned NOT NULL DEFAULT '0',
  -- `version` int(11) unsigned NOT NULL DEFAULT '1',
  -- `lastChated` int(11) unsigned NOT NULL DEFAULT '0',
  `join_at` timestamp NOT NULL,
  PRIMARY KEY (`id`),
  KEY `idx_name` (`name`(191)),
  KEY `idx_creator` (`creator`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


CREATE TABLE `users` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  -- `user_type` tinyint(4) NOT NULL DEFAULT '0',
  `access_hash` bigint(20) NOT NULL,
  -- `first_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  -- `last_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  `phone` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL,
  -- `country_code` varchar(3) COLLATE utf8mb4_unicode_ci NOT NULL,
  -- `verified` tinyint(4) NOT NULL DEFAULT '0',
  `about` varchar(512) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `state` int(11) NOT NULL DEFAULT '0',
  -- `is_bot` tinyint(1) NOT NULL DEFAULT '0',
  -- `account_days_ttl` int(11) NOT NULL DEFAULT '180',
  -- `photos` varchar(1024) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  -- `min` tinyint(4) NOT NULL DEFAULT '0',
  -- `restricted` tinyint(4) NOT NULL DEFAULT '0',
  -- `restriction_reason` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `deleted` tinyint(4) NOT NULL DEFAULT '0',
  `delete_reason` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `friends` (
  `user_id` bigint(20) NOT NULL,
  `peer_id` bigint(20) NOT NULL,
  -- `contact_user_id` int(11) NOT NULL,
  -- `contact_phone` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  -- `contact_first_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  -- `contact_last_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `peer_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  -- `mutual` tinyint(4) NOT NULL DEFAULT '0',
  -- `is_blocked` tinyint(1) NOT NULL DEFAULT '0',
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  -- `date2` int(11) NOT NULL,
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
  PRIMARY KEY (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `messages` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,

  `user_id` bigint(20) NOT NULL, -- equal sender or receiver
  `sender` bigint(20) NOT NULL,
  `receiver` bigint(20) NOT NULL, -- maybe group_id

  `msg_id` bigint(20) NOT NULL,
  `group_id` bigint(20) NOT NULL DEFAULT '0', -- 0 private msg, other group msg

  `message` text COLLATE utf8mb4_unicode_ci NOT NULL,
  `deleted` tinyint(4) NOT NULL DEFAULT '0',

  -- `send` tinyint(4) NOT NULL DEFAULT '0', -- 1: from user to peer 0: from peer to user

  `client_time` timestamp NOT NULL, -- sender and client_time can make one msg idempotent
  `msg_time` timestamp NOT NULL, -- time when server get msg from sender

  PRIMARY KEY (`id`),
  UNIQUE key `userid_and_sender_and_time`(`user_id`, `sender`, `send_time`),
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;