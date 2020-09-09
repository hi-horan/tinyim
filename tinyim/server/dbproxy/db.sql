
DROP DATABASE IF EXISTS tinyim;
CREATE DATABASE tinyim DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;
USE tinyim;

-- DROP TABLE IF EXISTS `groups`;

CREATE TABLE `groups` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,

  `group_id` bigint(20) NOT NULL,
  `name` varchar(256) COLLATE utf8mb4_bin NOT NULL DEFAULT '',
  `creator` bigint(20) unsigned NOT NULL,
  `status` tinyint(3) unsigned NOT NULL DEFAULT '1',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO groups(group_id, name, creator) VALUES(10000, "测试组1", 123);

CREATE TABLE `group_members` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,

  `group_id` bigint(20) NOT NULL,
  `group_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,

  `user_id` bigint(20) NOT NULL,
  `user_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,

  `join_at` timestamp NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO group_members(group_id, group_name, user_id, user_name) VALUES(10000, "测试组1", 123, "123");
INSERT INTO group_members(group_id, group_name, user_id, user_name) VALUES(10000, "测试组1", 1234, "1234");
INSERT INTO group_members(group_id, group_name, user_id, user_name) VALUES(10000, "测试组1", 12345, "12345");
INSERT INTO group_members(group_id, group_name, user_id, user_name) VALUES(10000, "测试组1", 123456, "123456");
INSERT INTO group_members(group_id, group_name, user_id, user_name) VALUES(10000, "测试组1", 1234567, "1234567");

DROP TABLE IF EXISTS `users`;
CREATE TABLE `users` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `user_id` bigint(20) NOT NULL,
  `name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL,
  `phone` varchar(32) COLLATE utf8mb4_unicode_ci NOT NULL,
  `access_hash` bigint(20) NOT NULL,
  `about` varchar(512) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `state` int(11) NOT NULL DEFAULT '0',
  `deleted` tinyint(4) NOT NULL DEFAULT '0',
  `delete_reason` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

insert into users(user_id, name, access_hash, phone) values(123, "123", 123, 123456789);
insert into users(user_id, name, access_hash, phone) values(1234, "1234", 123, 123456789);
insert into users(user_id, name, access_hash, phone) values(12345, "12345", 123, 123456789);
insert into users(user_id, name, access_hash, phone) values(123456, "123456", 123, 123456789);
insert into users(user_id, name, access_hash, phone) values(1234567, "1234567", 123, 123456789);

DROP TABLE IF EXISTS `friends`;
CREATE TABLE `friends` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `user_id` bigint(20) NOT NULL,
  `peer_id` bigint(20) NOT NULL,
  `peer_name` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `deleted` tinyint(1) NOT NULL DEFAULT '0',
  `created_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  key `user_id_and_peer_id`(`user_id`, `peer_id`),
  PRIMARY KEY (`id`)
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

  `client_time` timestamp NOT NULL, -- sender and client_time can make one msg idempotent
  `msg_time` timestamp NOT NULL, -- time when server get msg from sender

  PRIMARY KEY (`id`),
  UNIQUE key `userid_and_sender_and_time`(`user_id`, `sender`, `client_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO messages(user_id, sender, receiver, msg_id, group_id, message, client_time, msg_time) VALUES (123, 123, 1234, 1, 0, "first msg", FROM_UNIXTIME(1599455174), FROM_UNIXTIME(1599455174));
