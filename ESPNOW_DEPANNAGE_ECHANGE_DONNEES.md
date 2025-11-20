# 🔧 Dépannage : Échange de Données ESP-NOW

## 🚨 Problème : Le slave reçoit les beacons mais pas d'échange de données

### ✅ Correction Appliquée

**Problème identifié :** Le slave n'ajoutait pas le master comme peer ESP-NOW avant d'envoyer des données.

**Solution :** Le slave ajoute maintenant automatiquement le master comme peer quand il le découvre.

---

## 📊 Logs Attendus

### Sur le Slave

Quand le master est découvert, vous devriez voir :
```
Slave: Valid beacon from master 0xAABB on channel 6 (RSSI: -45)
Slave: Master discovered - device_id=0xAABB (AA:BB:CC:DD:EE:FF)
Slave: Master added as ESP-NOW peer on channel 6  ← NOUVEAU
Slave: Master info saved to NVS
Slave: Aligned to master channel 6
Slave: Send task started
```

Quand les données sont envoyées :
```
Slave: Data sent to master - seq=1, h0=123, h11=456  ← NOUVEAU (plus visible)
```

### Sur le Master

Quand le master reçoit des données :
```
Master: Data from slave 0xCCDD - seq=1, h0=123, h11=456 (RSSI: -50)
Master: New slave discovered - 0xCCDD (AA:BB:CC:DD:EE:FF)
Master: Slave added as ESP-NOW peer on channel 6  ← NOUVEAU
```

---

## 🔍 Vérifications à Faire

### 1. Vérifier que le slave a ajouté le master comme peer

**Sur le slave**, après la découverte, vous devriez voir :
```
Slave: Master added as ESP-NOW peer on channel 6
```

Si vous ne voyez pas ce log, le peer n'a pas été ajouté.

### 2. Vérifier que le slave envoie des données

**Sur le slave**, toutes les 2 secondes, vous devriez voir :
```
Slave: Data sent to master - seq=X, h0=XXX, h11=XXX
```

Si vous ne voyez pas ce log :
- Vérifiez que `master_found = true`
- Vérifiez que la tâche `send_task` est démarrée
- Vérifiez les logs d'erreur

### 3. Vérifier que le master reçoit les données

**Sur le master**, vous devriez voir :
```
Master: Data from slave 0xXXXX - seq=X, h0=XXX, h11=XXX (RSSI: XX)
```

Si vous ne voyez pas ce log :
- Vérifiez que le master est démarré (`app_start 10`)
- Vérifiez que le `master_id` correspond
- Vérifiez les logs d'erreur

### 4. Vérifier les erreurs d'envoi

**Sur le slave**, si l'envoi échoue :
```
Slave: Send failed (err=X), failure_count=Y
```

**Codes d'erreur ESP-NOW :**
- `ESP_ERR_ESPNOW_NOT_FOUND` (-1) : Peer non trouvé → Le slave doit réajouter le peer
- `ESP_ERR_ESPNOW_IF` (-2) : Interface WiFi non initialisée
- `ESP_ERR_ESPNOW_ARG` (-3) : Argument invalide
- `ESP_ERR_ESPNOW_INTERNAL` (-4) : Erreur interne
- `ESP_ERR_ESPNOW_NO_MEM` (-5) : Plus de mémoire
- `ESP_ERR_ESPNOW_NOT_FOUND` (-6) : Peer non trouvé

---

## 🛠️ Actions Correctives

### Si le slave n'envoie pas de données

1. **Vérifier que le master est trouvé** :
   ```
   # Les logs doivent montrer :
   Slave: Master discovered - device_id=0xXXXX
   Slave: Master added as ESP-NOW peer on channel X
   ```

2. **Vérifier que la tâche send_task est démarrée** :
   ```
   # Les logs doivent montrer :
   Slave: Send task started
   ```

3. **Vérifier les erreurs d'envoi** :
   ```
   # Si vous voyez :
   Slave: Send failed (err=-1), failure_count=1
   ```
   → Le peer n'est pas ajouté, le slave devrait le réajouter automatiquement

### Si le master ne reçoit pas les données

1. **Vérifier que le master est démarré** :
   ```
   app_status
   # L'app 10 doit être RUNNING
   ```

2. **Vérifier que le master_id correspond** :
   ```
   # Sur le master
   espnow config show

   # Sur le slave
   espnow config show

   # Les deux doivent avoir le même master_id
   ```

3. **Vérifier que le master écoute** :
   ```
   # Les logs doivent montrer :
   Master: ESP-NOW initialized successfully
   Master: Tasks created - ready to receive slave data
   ```

---

## 📝 Checklist de Diagnostic

### Sur le Slave

- [ ] Le slave a trouvé le master : `Slave: Master discovered`
- [ ] Le peer a été ajouté : `Slave: Master added as ESP-NOW peer`
- [ ] La tâche send_task est démarrée : `Slave: Send task started`
- [ ] Les données sont envoyées : `Slave: Data sent to master - seq=X`
- [ ] Pas d'erreurs d'envoi : `Slave: Send failed`

### Sur le Master

- [ ] Le master est démarré : `app_status` montre app 10 RUNNING
- [ ] Le master_id correspond : `espnow config show` (identique au slave)
- [ ] Le master écoute : `Master: Tasks created - ready to receive slave data`
- [ ] Les données sont reçues : `Master: Data from slave 0xXXXX`
- [ ] Le slave est ajouté comme peer : `Master: Slave added as ESP-NOW peer`

---

## 🔄 Redémarrage pour Forcer la Reconfiguration

Si les données ne s'échangent toujours pas :

### Sur le Slave

```bash
# 1. Arrêter l'app
app_stop 11

# 2. Attendre 1 seconde
# (délai automatique)

# 3. Redémarrer l'app
app_start 11

# 4. Vérifier les logs
# Vous devriez voir :
# - "Slave: Master discovered"
# - "Slave: Master added as ESP-NOW peer"
# - "Slave: Data sent to master"
```

### Sur le Master

```bash
# 1. Arrêter l'app
app_stop 10

# 2. Attendre 1 seconde
# (délai automatique)

# 3. Redémarrer l'app
app_start 10

# 4. Vérifier les logs
# Vous devriez voir :
# - "Master: Beacon #X sent"
# - "Master: Data from slave"
```

---

## 🎯 Résultat Attendu Après Correction

### Séquence Complète

**1. Slave découvre le master :**
```
Slave: Valid beacon from master 0xAABB on channel 6 (RSSI: -45)
Slave: Master discovered - device_id=0xAABB
Slave: Master added as ESP-NOW peer on channel 6  ← CRITIQUE
Slave: Master info saved to NVS
```

**2. Slave envoie des données :**
```
Slave: Data sent to master - seq=1, h0=123, h11=456
```

**3. Master reçoit les données :**
```
Master: Data from slave 0xCCDD - seq=1, h0=123, h11=456 (RSSI: -50)
Master: New slave discovered - 0xCCDD
Master: Slave added as ESP-NOW peer on channel 6
```

**4. Master envoie ACK :**
```
# (Pas de log explicite, mais le slave devrait recevoir l'ACK)
```

**5. Slave reçoit ACK :**
```
Slave: ACK received for seq=1
```

**6. Répétition toutes les 2 secondes :**
```
Slave: Data sent to master - seq=2, h0=234, h11=567
Master: Data from slave 0xCCDD - seq=2, h0=234, h11=567 (RSSI: -50)
```

---

## ⚠️ Notes Importantes

1. **ESP-NOW nécessite un peer** : Pour envoyer des données, le device doit avoir ajouté le destinataire comme peer.

2. **Le canal doit correspondre** : Le slave et le master doivent être sur le même canal WiFi.

3. **Le master_id doit correspondre** : Le filtrage se fait par `master_id`, donc il doit être identique.

4. **Les logs sont maintenant plus verbeux** : Vous devriez voir clairement quand les données sont envoyées et reçues.

---

## 🆘 Si le Problème Persiste

1. **Vérifier la version du firmware** : Recompiler avec les dernières modifications
2. **Vérifier les logs complets** : Regarder tous les logs, pas seulement les erreurs
3. **Tester avec un seul slave** : Simplifier pour isoler le problème
4. **Vérifier la distance** : ESP-NOW a une portée limitée (~100m en ligne de vue)
5. **Vérifier les interférences** : D'autres réseaux WiFi peuvent interférer

---

## 📞 Commandes Utiles pour le Diagnostic

```bash
# Voir la configuration
espnow config show

# Voir l'état des apps
app_status

# Voir tous les logs (dans Serial Monitor)
# Filtrer par "Slave:" ou "Master:"

# Redémarrer une app
app_restart 10  # Master
app_restart 11  # Slave
```

