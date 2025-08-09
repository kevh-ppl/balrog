#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/acl.h>
#include <sys/types.h>

int set_acl_user_rw(const char *path, const char *username) {
    acl_t acl = acl_get_file(path, ACL_TYPE_ACCESS);
    if (acl == NULL) {
        perror("acl_get_file");
        return -1;
    }

    struct passwd *pw = getpwnam(username);
    if (!pw) {
        fprintf(stderr, "User %s not found\n", username);
        acl_free(acl);
        return -1;
    }
    uid_t uid = pw->pw_uid;

    acl_entry_t entry;
    int entry_id = ACL_FIRST_ENTRY;
    acl_entry_t found_entry = NULL;

    // Buscar entrada existente para ese usuario
    while (acl_get_entry(acl, entry_id, &entry) == 1) {
        entry_id = ACL_NEXT_ENTRY;

        acl_tag_t tag_type;
        if (acl_get_tag_type(entry, &tag_type) != 0) continue;

        if (tag_type == ACL_USER) {
            uid_t *qual_uid = (uid_t *)acl_get_qualifier(entry);
            if (qual_uid) {
                if (*qual_uid == uid) {
                    found_entry = entry;
                    acl_free(qual_uid);
                    break;
                }
                acl_free(qual_uid);
            }
        }
    }

    if (!found_entry) {
        printf("No existe acl\n");
        // No existe: crear nueva entrada
        if (acl_create_entry(&acl, &found_entry) != 0) {
            perror("acl_create_entry");
            acl_free(acl);
            return -1;
        }
        if (acl_set_tag_type(found_entry, ACL_USER) != 0) {
            perror("acl_set_tag_type");
            acl_free(acl);
            return -1;
        }
        if (acl_set_qualifier(found_entry, &uid) != 0) {
            perror("acl_set_qualifier");
            acl_free(acl);
            return -1;
        }
    }

    // Modificar permisos para lectura/escritura
    acl_permset_t permset;
    if (acl_get_permset(found_entry, &permset) != 0) {
        perror("acl_get_permset");
        acl_free(acl);
        return -1;
    }
    acl_clear_perms(permset);
    acl_add_perm(permset, ACL_READ);   // ahhh
    acl_add_perm(permset, ACL_WRITE);  // just read
    if (acl_set_permset(found_entry, permset) != 0) {
        perror("acl_set_permset");
        acl_free(acl);
        return -1;
    }

    char *text = acl_to_text(acl, NULL);
    if (text) {
        printf("ACL actual:\n%s\n", text);
        acl_free(text);
    }

    // Guardar cambios
    if (acl_set_file(path, ACL_TYPE_ACCESS, acl) != 0) {
        perror("acl_set_file");
        acl_free(acl);
        return -1;
    }

    acl_free(acl);
    return 0;
}